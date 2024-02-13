#define MS_CLASS "RTC::RtpPacketsPlayerMediaFragment"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/RtpPacket.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include "absl/container/flat_hash_map.h"
#include <list>
#include <variant>
#include <queue>

namespace {

enum class PlayTaskType {
    Started,
    Frame,
    Finished
};

class PlayTask
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

}

namespace RTC
{

class RtpPacketsPlayerMediaFragment::TimerCallback : public MediaTimerCallback
{
public:
    TimerCallback(const std::weak_ptr<MediaTimer>& timerRef,
                  const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                  std::unique_ptr<MediaFrameDeserializer> deserializer,
                  uint32_t ssrc, uint32_t clockRate, uint8_t payloadType, uint64_t mediaId,
                  const void* userData);
    ~TimerCallback() final;
    uint64_t SetTimerId(uint64_t timerId); // return previous ID
    uint64_t GetTimerId() const { return _timerId.load(); }
    uint64_t GetMediaId() const { return _mediaId; }
    void SetPlayerCallback(const std::shared_ptr<RtpPacketsPlayerCallback>& playerCallback);
    bool Parse(const RtpCodecMimeType& mime, const std::shared_ptr<MemoryBuffer>& buffer);
    void PlayFrames();
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    void Enque(PlayTask task);
    void EnqueStartEvent() { Enque(true); }
    void EnqueFinishEvent() { Enque(false); }
    void EnqueFrame(size_t trackIndex, const std::shared_ptr<const RTC::MediaFrame>& frame);
    void ConvertToRtpAndSend(size_t trackIndex,
                             const std::shared_ptr<RtpPacketsPlayerCallback>& callback,
                             const std::shared_ptr<const RTC::MediaFrame>& frame);
    RtpPacket* CreatePacket(size_t trackIndex, const std::shared_ptr<const MediaFrame>& frame) const;
private:
    const std::weak_ptr<MediaTimer> _timerRef;
    const std::weak_ptr<RtpPacketsPlayerCallback> _playerCallbackRef;
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const uint8_t _payloadType;
    const uint64_t _mediaId;
    const void* const _userData;
    std::atomic<uint64_t> _timerId = 0ULL;
    uint32_t _previousTimestamp = 0U;
    // key is track number, value - packetizer instance
    ProtectedObj<absl::flat_hash_map<size_t, std::unique_ptr<RtpPacketizer>>> _packetizers;
    ProtectedObj<std::queue<PlayTask>> _tasks;
};

RtpPacketsPlayerMediaFragment::RtpPacketsPlayerMediaFragment(const std::shared_ptr<MediaTimer>& timer,
                                                             const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                                                             std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                             uint32_t ssrc, uint32_t clockRate,
                                                             uint8_t payloadType, uint64_t mediaId,
                                                             const void* userData)
    : _timerCallback(std::make_shared<TimerCallback>(timer, playerCallbackRef,
                                                     std::move(deserializer),
                                                     ssrc, clockRate,
                                                     payloadType, mediaId, userData))
    , _timer(timer)
{
    _timerCallback->SetTimerId(_timer->RegisterTimer(_timerCallback));
    MS_ASSERT(_timerCallback->GetTimerId(), "failed to register in timer");
}

RtpPacketsPlayerMediaFragment::~RtpPacketsPlayerMediaFragment()
{
    _timer->UnregisterTimer(_timerCallback->SetTimerId(0UL));
}

bool RtpPacketsPlayerMediaFragment::Parse(const RtpCodecMimeType& mime,
                                          const std::shared_ptr<MemoryBuffer>& buffer)
{
    return _timerCallback->Parse(mime, buffer);
}

void RtpPacketsPlayerMediaFragment::PlayFrames()
{
    _timerCallback->PlayFrames();
}

bool RtpPacketsPlayerMediaFragment::IsPlaying() const
{
    return _timer->IsStarted(_timerCallback->GetTimerId());
}

uint64_t RtpPacketsPlayerMediaFragment::GetMediaId() const
{
    return _timerCallback->GetMediaId();
}

RtpPacketsPlayerMediaFragment::TimerCallback::TimerCallback(const std::weak_ptr<MediaTimer>& timerRef,
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
{
    MS_ASSERT(!_timerRef.expired(), "timer must not be null");
    MS_ASSERT(_deserializer, "deserializer must not be null");
}

RtpPacketsPlayerMediaFragment::TimerCallback::~TimerCallback()
{
    LOCK_WRITE_PROTECTED_OBJ(_packetizers);
    _packetizers->clear();
}

uint64_t RtpPacketsPlayerMediaFragment::TimerCallback::SetTimerId(uint64_t timerId)
{
    const auto oldTimerId = _timerId.exchange(timerId);
    if (oldTimerId) {
        if (const auto timer = _timerRef.lock()) {
            timer->Stop(oldTimerId);
        }
    }
    return oldTimerId;
}

bool RtpPacketsPlayerMediaFragment::TimerCallback::Parse(const RtpCodecMimeType& mime,
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

void RtpPacketsPlayerMediaFragment::TimerCallback::OnEvent()
{
    std::optional<PlayTask> task;
    {
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        if (!_tasks->empty()) {
            task = std::move(_tasks->front());
            _tasks->pop();
        }
    }
    if (task.has_value()) {
        switch (task->GetType()) {
            case PlayTaskType::Started:
                _previousTimestamp = 0U;
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
                _previousTimestamp = 0U;
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

void RtpPacketsPlayerMediaFragment::TimerCallback::PlayFrames()
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
                        EnqueStartEvent();
                    }
                    EnqueFrame(it->first, frame);
                }
                if (!MaybeOk(result)) {
                    MS_ERROR_STD("read of deserialized frames was failed: %s", ToString(result));
                    break;
                }
            }
            if (started) {
                EnqueFinishEvent();
            }
        }
    }
}

void RtpPacketsPlayerMediaFragment::TimerCallback::Enque(PlayTask task)
{
    if (const auto timer = _timerRef.lock()) {
        const auto startTimer = PlayTaskType::Started == task.GetType();
        {
            LOCK_WRITE_PROTECTED_OBJ(_tasks);
            _tasks->push(std::move(task));
        }
        if (startTimer) {
            timer->SetTimeout(GetTimerId(), 0ULL);
            timer->Start(GetTimerId(), false);
        }
    }
}

void RtpPacketsPlayerMediaFragment::TimerCallback::
    EnqueFrame(size_t trackIndex, const std::shared_ptr<const RTC::MediaFrame>& frame)
{
    if (frame) {
        Enque(PlayTask(trackIndex, frame));
    }
}

void RtpPacketsPlayerMediaFragment::TimerCallback::
    ConvertToRtpAndSend(size_t trackIndex,
                        const std::shared_ptr<RtpPacketsPlayerCallback>& callback,
                        const std::shared_ptr<const RTC::MediaFrame>& frame)
{
    if (frame && callback) {
        if (const auto timerId = GetTimerId()) {
            if (const auto timer = _timerRef.lock()) {
                const auto timestamp = frame->GetTimestamp();
                if (0U == timestamp) {
                    //timer->SetTimeout(timerId, 20U); // 20ms
                }
                else {
                    // TODO: rewrite timeout calculations
                    // TODO: review timestamp type, must me more clarified (ms/us/nanosecs)
                    const auto diff = (timestamp - _previousTimestamp) * 1000;
                    timer->SetTimeout(timerId, diff / _clockRate);
                }
                _previousTimestamp = timestamp;
                if (const auto packet = CreatePacket(trackIndex, frame)) {
                    callback->OnPlay(frame->GetTimestamp(), packet, _mediaId, _userData);
                }
            }
        }
    }
}

RtpPacket* RtpPacketsPlayerMediaFragment::TimerCallback::CreatePacket(size_t trackIndex,
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

} // namespace RTC

namespace {

PlayTask::PlayTask(bool started)
    : _data(started)
{
}

PlayTask::PlayTask(size_t trackIndex, const std::shared_ptr<const RTC::MediaFrame>& frame)
    : _data(std::make_pair(trackIndex, frame))
{
}

PlayTaskType PlayTask::GetType() const
{
    if (const auto started = std::get_if<bool>(&_data)) {
        return *started ? PlayTaskType::Started : PlayTaskType::Finished;
    }
    return PlayTaskType::Frame;
}

std::shared_ptr<const RTC::MediaFrame> PlayTask::GetFrame() const
{
    if (const auto frame = std::get_if<FrameInfo>(&_data)) {
        return frame->second;
    }
    return nullptr;
}

std::optional<size_t> PlayTask::GetTrackIndex() const
{
    if (const auto frame = std::get_if<FrameInfo>(&_data)) {
        return frame->first;
    }
    return std::nullopt;
}

}
