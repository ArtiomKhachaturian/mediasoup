#define MS_CLASS "RTC::RtpPacketsMediaFragmentPlayer"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsMediaFragmentPlayer.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"
#include <queue>
#include <variant>

namespace {

enum class PlayTaskType {
    Started,
    Frame,
    Finished
};

}

namespace RTC
{

class RtpPacketsMediaFragmentPlayer::PlayTask
{
    // 1st is track number
    using FrameInfo = std::pair<size_t, std::shared_ptr<const RTC::MediaFrame>>;
public:
    PlayTask(bool started);
    PlayTask(size_t trackIndex, const std::shared_ptr<const RTC::MediaFrame>& frame);
    PlayTask(const PlayTask&) = default;
    PlayTask(PlayTask&&) = default;
    PlayTaskType GetType() const;
    std::shared_ptr<const RTC::MediaFrame> GetFrame() const;
    std::optional<size_t> GetTrackIndex() const;
    PlayTask& operator = (const PlayTask&) = default;
    PlayTask& operator = (PlayTask&&) = default;
private:
    std::variant<bool, FrameInfo> _data;
};

class RtpPacketsMediaFragmentPlayer::PlayTaskQueue
{
public:
    PlayTaskQueue() = default;
    std::optional<PlayTask> Pop();
    void Enque(PlayTask task);
    void EnqueStartEvent();
    void EnqueFinishEvent();
    void EnqueFrame(size_t trackIndex, const std::shared_ptr<const MediaFrame>& frame);
private:
    ProtectedObj<std::queue<PlayTask>> _tasks;
};

RtpPacketsMediaFragmentPlayer::RtpPacketsMediaFragmentPlayer(const std::weak_ptr<MediaTimer>& timerRef,
                                                             const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                                                             std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                             uint32_t ssrc, uint32_t clockRate,
                                                             uint8_t payloadType, uint64_t mediaId,
                                                             const void* userData)
    : _timerRef(timerRef)
    , _playerCallbackRef(playerCallbackRef)
    , _deserializer(std::move(deserializer))
    , _ssrc(ssrc)
    , _clockRate(clockRate)
    , _payloadType(payloadType)
    , _mediaId(mediaId)
    , _userData(userData)
    , _tasksQueue(std::make_unique<PlayTaskQueue>())
{
    MS_ASSERT(!_timerRef.expired(), "timer must not be null");
    MS_ASSERT(_deserializer, "deserializer must not be null");
}

RtpPacketsMediaFragmentPlayer::~RtpPacketsMediaFragmentPlayer()
{
    LOCK_WRITE_PROTECTED_OBJ(_packetizers);
    _packetizers->clear();
}

uint64_t RtpPacketsMediaFragmentPlayer::SetTimerId(uint64_t timerId)
{
    const auto oldTimerId = _timerId.exchange(timerId);
    if (oldTimerId) {
        if (const auto timer = _timerRef.lock()) {
            timer->Stop(oldTimerId);
        }
    }
    return oldTimerId;
}

bool RtpPacketsMediaFragmentPlayer::Parse(const RtpCodecMimeType& mime,
                                          const std::shared_ptr<MemoryBuffer>& buffer)
{
    bool ok = false;
    if (buffer && GetTimerId() && !_playerCallbackRef.expired()) {
        const auto result = _deserializer->AddBuffer(buffer);
        if (MaybeOk(result)) {
            if (const auto tracksCount = _deserializer->GetTracksCount()) {
                size_t acceptedTracksCount = 0UL;
                for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                    const auto trackMime = _deserializer->GetTrackMimeType(trackIndex);
                    if (trackMime.has_value() && trackMime.value() == mime) {
                        std::unique_ptr<RtpPacketizer> packetizer;
                        switch (mime.GetSubtype()) {
                            case RtpCodecMimeType::Subtype::OPUS:
                                packetizer = std::make_unique<RtpPacketizerOpus>();
                                break;
                            default:
                                break;
                        }
                        if (packetizer) {
                            _deserializer->SetClockRate(trackIndex, _clockRate);
                            LOCK_WRITE_PROTECTED_OBJ(_packetizers);
                            _packetizers->insert(std::make_pair(trackIndex, std::move(packetizer)));
                            ++acceptedTracksCount;
                        }
                        else {
                            MS_ERROR_STD("packetizer for [%s] not yet implemented", mime.ToString().c_str());
                        }
                    }
                }
                ok = acceptedTracksCount > 0UL;
            }
            else {
                MS_ERROR_STD("deserialized media buffer has no media tracks");
            }
        }
        else {
            MS_ERROR_STD("media buffer deserialization was failed: %s", ToString(result));
        }
    }
    return ok;
}

void RtpPacketsMediaFragmentPlayer::OnEvent()
{
    const auto task = _tasksQueue->Pop();
    if (task.has_value()) {
        switch (task->GetType()) {
            case PlayTaskType::Started:
                _previousTimestamp = webrtc::Timestamp::Zero();
                if (const auto playerCallback = _playerCallbackRef.lock()) {
                    playerCallback->OnPlayStarted(_ssrc, _mediaId, _userData);
                }
                break;
            case PlayTaskType::Frame:
                if (const auto frame = task->GetFrame()) {
                    const auto trackIndex = task->GetTrackIndex();
                    MS_ASSERT(trackIndex.has_value(), "no information about trackIndex");
                    ConvertToRtpAndSend(trackIndex.value(), _playerCallbackRef.lock(), frame);
                }
                break;
            case PlayTaskType::Finished:
                _previousTimestamp = webrtc::Timestamp::Zero();
                if (const auto timer = _timerRef.lock()) {
                    timer->Stop(GetTimerId());
                }
                if (const auto playerCallback = _playerCallbackRef.lock()) {
                    playerCallback->OnPlayFinished(_ssrc, _mediaId, _userData);
                }
                break;
        }
    }
}

void RtpPacketsMediaFragmentPlayer::PlayFrames()
{
    if (GetTimerId()) {
        LOCK_READ_PROTECTED_OBJ(_packetizers);
        if (!_packetizers->empty()) {
            MediaFrameDeserializeResult result = MediaFrameDeserializeResult::Success;
            bool started = false;
            for (auto it = _packetizers->begin(); it != _packetizers->end(); ++it) {
                for (const auto& frame : _deserializer->ReadNextFrames(it->first, &result)) {
                    if (!started) {
                        started = true;
                        _tasksQueue->EnqueStartEvent();
                        if (const auto timer = _timerRef.lock()) {
                            timer->SetTimeout(GetTimerId(), 0ULL);
                            timer->Start(GetTimerId(), false);
                        }
                    }
                    _tasksQueue->EnqueFrame(it->first, frame);
                }
                if (!MaybeOk(result)) {
                    MS_ERROR_STD("read of deserialized frames was failed: %s", ToString(result));
                    break;
                }
            }
            if (started) {
                _tasksQueue->EnqueFinishEvent();
            }
        }
    }
}

void RtpPacketsMediaFragmentPlayer::ConvertToRtpAndSend(size_t trackIndex,
                                                        const std::shared_ptr<RtpPacketsPlayerCallback>& callback,
                                                        const std::shared_ptr<const RTC::MediaFrame>& frame)
{
    if (frame && callback) {
        if (const auto timerId = GetTimerId()) {
            if (const auto timer = _timerRef.lock()) {
                const auto timestamp = frame->GetTimestamp();
                if (timestamp > _previousTimestamp) {
                    const auto diff = timestamp - _previousTimestamp;
                    timer->SetTimeout(timerId, diff.ms<uint32_t>());
                    _previousTimestamp = timestamp;
                }
                if (const auto packet = CreatePacket(trackIndex, frame)) {
                    callback->OnPlay(frame->GetRtpTimestamp(), packet, _mediaId, _userData);
                }
            }
        }
    }
}

RtpPacket* RtpPacketsMediaFragmentPlayer::CreatePacket(size_t trackIndex,
                                                       const std::shared_ptr<const MediaFrame>& frame) const
{
    if (frame) {
        LOCK_READ_PROTECTED_OBJ(_packetizers);
        const auto it = _packetizers->find(trackIndex);
        if (it != _packetizers->end()) {
            if (const auto packet = it->second->AddFrame(frame)) {
                packet->SetSsrc(_ssrc);
                packet->SetPayloadType(_payloadType);
                return packet;
            }
        }
    }
    return nullptr;
}

RtpPacketsMediaFragmentPlayer::PlayTask::PlayTask(bool started)
    : _data(started)
{
}

RtpPacketsMediaFragmentPlayer::PlayTask::PlayTask(size_t trackIndex,
                                                  const std::shared_ptr<const RTC::MediaFrame>& frame)
    : _data(std::make_pair(trackIndex, frame))
{
}

PlayTaskType RtpPacketsMediaFragmentPlayer::PlayTask::GetType() const
{
    if (const auto started = std::get_if<bool>(&_data)) {
        return *started ? PlayTaskType::Started : PlayTaskType::Finished;
    }
    return PlayTaskType::Frame;
}

std::shared_ptr<const MediaFrame> RtpPacketsMediaFragmentPlayer::PlayTask::GetFrame() const
{
    if (const auto frame = std::get_if<FrameInfo>(&_data)) {
        return frame->second;
    }
    return nullptr;
}

std::optional<size_t> RtpPacketsMediaFragmentPlayer::PlayTask::GetTrackIndex() const
{
    if (const auto frame = std::get_if<FrameInfo>(&_data)) {
        return frame->first;
    }
    return std::nullopt;
}

std::optional<RtpPacketsMediaFragmentPlayer::PlayTask> RtpPacketsMediaFragmentPlayer::PlayTaskQueue::Pop()
{
    std::optional<PlayTask> task;
    LOCK_WRITE_PROTECTED_OBJ(_tasks);
    if (!_tasks->empty()) {
        task = std::move(_tasks->front());
        _tasks->pop();
    }
    return task;
}

void RtpPacketsMediaFragmentPlayer::PlayTaskQueue::Enque(PlayTask task)
{
    LOCK_WRITE_PROTECTED_OBJ(_tasks);
    _tasks->push(std::move(task));
}

void RtpPacketsMediaFragmentPlayer::PlayTaskQueue::EnqueStartEvent()
{
    Enque(true);
}

void RtpPacketsMediaFragmentPlayer::PlayTaskQueue::EnqueFinishEvent()
{
    Enque(false);
}

void RtpPacketsMediaFragmentPlayer::PlayTaskQueue::EnqueFrame(size_t trackIndex,
                                                              const std::shared_ptr<const MediaFrame>& frame)
{
    if (frame) {
        Enque(PlayTask(trackIndex, frame));
    }
}

} // namespace RTC
