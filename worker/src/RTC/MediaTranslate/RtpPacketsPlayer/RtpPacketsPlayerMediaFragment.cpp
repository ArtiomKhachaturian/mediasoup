#define MS_CLASS "RTC::RtpPacketsPlayerMediaFragment"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpPacketsInfoProvider.hpp"
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
                  std::unique_ptr<MediaFrameDeserializer> deserializer,
                  uint32_t ssrc, uint64_t mediaId,
                  const void* userData);
    ~TimerCallback() final;
    uint64_t SetTimerId(uint64_t timerId); // return previous ID
    uint64_t GetTimerId() const { return _timerId.load(); }
    void SetPlayerCallback(const std::shared_ptr<RtpPacketsPlayerCallback>& playerCallback);
    bool Parse(const RtpCodecMimeType& mime,
               const RtpPacketsInfoProvider* packetsInfoProvider,
               const std::shared_ptr<MemoryBuffer>& buffer);
    void PlayFrames();
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    void Enque(PlayTask task);
    void EnqueStartEvent() { Enque(true); }
    void EnqueFinishEvent() { Enque(false); }
    void EnqueFrame(size_t trackIndex, const std::shared_ptr<const RTC::MediaFrame>& frame);
    RtpPacket* CreatePacket(size_t trackIndex, const std::shared_ptr<const MediaFrame>& frame) const;
private:
    const std::weak_ptr<MediaTimer> _timerRef;
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    const uint32_t _ssrc;
    const uint64_t _mediaId;
    const void* const _userData;
    std::atomic<uint64_t> _timerId = 0ULL;
    std::shared_ptr<RtpPacketsPlayerCallback> _playerCallback;
    // key is track number, value - packetizer instance
    ProtectedObj<absl::flat_hash_map<size_t, std::unique_ptr<RtpPacketizer>>> _packetizers;
    ProtectedObj<std::queue<PlayTask>> _tasks;
};

RtpPacketsPlayerMediaFragment::RtpPacketsPlayerMediaFragment(const std::shared_ptr<MediaTimer>& timer,
                                                             std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                             uint32_t ssrc, uint64_t mediaId,
                                                             const void* userData)
    : _timerCallback(std::make_shared<TimerCallback>(timer, std::move(deserializer),
                                                     ssrc, mediaId, userData))
    , _timer(timer)
{
    _timerCallback->SetTimerId(_timer->RegisterTimer(_timerCallback));
    MS_ASSERT(_timerCallback->GetTimerId(), "failed to register in timer");
}

RtpPacketsPlayerMediaFragment::~RtpPacketsPlayerMediaFragment()
{
    SetPlayerCallback(nullptr);
    _timer->UnregisterTimer(_timerCallback->SetTimerId(0UL));
}

void RtpPacketsPlayerMediaFragment::
    SetPlayerCallback(const std::shared_ptr<RtpPacketsPlayerCallback>& playerCallback)
{
    _timerCallback->SetPlayerCallback(playerCallback);
}

bool RtpPacketsPlayerMediaFragment::Parse(const RtpCodecMimeType& mime,
                                          const RtpPacketsInfoProvider* packetsInfoProvider,
                                          const std::shared_ptr<MemoryBuffer>& buffer)
{
    return _timerCallback->Parse(mime, packetsInfoProvider, buffer);
}

void RtpPacketsPlayerMediaFragment::PlayFrames()
{
    _timerCallback->PlayFrames();
}

bool RtpPacketsPlayerMediaFragment::IsPlaying() const
{
    return _timer->IsStarted(_timerCallback->GetTimerId());
}

RtpPacketsPlayerMediaFragment::TimerCallback::TimerCallback(const std::weak_ptr<MediaTimer>& timerRef,
                                                            std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                            uint32_t ssrc, uint64_t mediaId,
                                                            const void* userData)
    : _timerRef(timerRef)
    , _deserializer(std::move(deserializer))
    , _ssrc(ssrc)
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

void RtpPacketsPlayerMediaFragment::TimerCallback::
    SetPlayerCallback(const std::shared_ptr<RtpPacketsPlayerCallback>& playerCallback)
{
    std::atomic_store(&_playerCallback, playerCallback);
}

bool RtpPacketsPlayerMediaFragment::TimerCallback::Parse(const RtpCodecMimeType& mime,
                                                         const RtpPacketsInfoProvider* packetsInfoProvider,
                                                         const std::shared_ptr<MemoryBuffer>& buffer)
{
    bool ok = false;
    if (buffer && packetsInfoProvider && GetTimerId() && std::atomic_load(&_playerCallback)) {
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
                            _deserializer->SetClockRate(trackIndex, packetsInfoProvider->GetClockRate(_ssrc));
                            LOCK_WRITE_PROTECTED_OBJ(_packetizers);
                            _packetizers->insert(std::make_pair(trackIndex, std::move(packetizer)));
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
    if (const auto timerId = GetTimerId()) {
        std::optional<PlayTask> task;
        {
            LOCK_WRITE_PROTECTED_OBJ(_tasks);
            if (!_tasks->empty()) {
                task = std::move(_tasks->front());
                _tasks->pop();
            }
        }
        if (task.has_value()) {
            if (const auto playerCallback = std::atomic_load(&_playerCallback)) {
                switch (task->GetType()) {
                    case PlayTaskType::Started:
                        playerCallback->OnPlayStarted(_ssrc, _mediaId, _userData);
                        break;
                    case PlayTaskType::Frame:
                        if (const auto frame = task->GetFrame()) {
                            const auto ts = frame->GetTimestamp();
                            bool depacketize = true;
                            if (0U == ts) { // 1st
                                if (const auto timer = _timerRef.lock()) {
                                    // TODO: rewrite timeout calculations
                                    // TODO: review timestamp type, must me more clarified (ms/us/nanosecs)
                                    timer->SetTimeout(timerId, 20U); // 20ms
                                    //const auto diff = (frame->GetTimestamp()  - _playoutOffset) * 1000;
                                    //_playoutOffset = frame->GetTimestamp();
                                    //timer->SetTimeout(timerId, diff / GetClockRate());
                                }
                                else {
                                    depacketize = false;
                                }
                            }
                            if (depacketize) {
                                const auto trackIndex = task->GetTrackIndex();
                                MS_ASSERT(trackIndex.has_value(), "no information about trackIndex");
                                if (const auto packet = CreatePacket(trackIndex.value(), frame)) {
                                    playerCallback->OnPlay(frame->GetTimestamp(), packet,
                                                           _mediaId, _userData);
                                }
                            }
                        }
                        break;
                    case PlayTaskType::Finished:
                        if (const auto timer = _timerRef.lock()) {
                            timer->Stop(timerId);
                        }
                        playerCallback->OnPlayFinished(_ssrc, _mediaId, _userData);
                        break;
                }
            }
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
                if (!IsOk(result)) {
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

RtpPacket* RtpPacketsPlayerMediaFragment::TimerCallback::CreatePacket(size_t trackIndex,
                                                                      const std::shared_ptr<const MediaFrame>& frame) const
{
    if (frame) {
        LOCK_READ_PROTECTED_OBJ(_packetizers);
        const auto it = _packetizers->find(trackIndex);
        if (it != _packetizers->end()) {
            if (const auto packet = it->second->AddFrame(frame)) {
                packet->SetSsrc(_ssrc);
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
