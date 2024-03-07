#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "ProtectedObj.hpp"
#include <atomic>
#include <memory>
#include <optional>
#include <queue>

namespace RTC
{

class MediaTimer;
class MediaFrameDeserializer;
class RtpTranslatedPacket;
class RtpPacketsPlayerStreamCallback;
class RtpCodecMimeType;

class RtpPacketsPlayerMediaFragmentQueue : public MediaTimerCallback
{
    class Task;
    class StartTask;
    class MediaFrameTask;
public:
    static std::shared_ptr<RtpPacketsPlayerMediaFragmentQueue> Create(uint64_t mediaId, uint64_t mediaSourceId,
                                                                      const std::weak_ptr<MediaTimer>& timerRef,
                                                                      std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                                      RtpPacketsPlayerStreamCallback* callback);
    ~RtpPacketsPlayerMediaFragmentQueue() final;
    uint64_t GetMediaId() const { return _mediaId; }
    uint64_t GetMediaSourceId() const { return _mediaSourceId; }
    void Start(size_t trackIndex, uint32_t clockRate);
    void Stop();
    size_t GetTracksCount() const;
    std::optional<RtpCodecMimeType> GetTrackType(size_t trackIndex) const;
    // impl. of MediaTimerCallback
    void OnCallbackRegistered(uint64_t timerId, bool registered) final;
    void OnEvent(uint64_t timerId) final;
private:
    RtpPacketsPlayerMediaFragmentQueue(uint64_t mediaId, uint64_t mediaSourceId,
                                       const std::weak_ptr<MediaTimer>& timerRef,
                                       std::unique_ptr<MediaFrameDeserializer> deserializer,
                                       RtpPacketsPlayerStreamCallback* callback);
    void SetTimerId(uint64_t desiredTimerId, uint64_t expectedTimerId);
    uint64_t GetTimerId() const { return _timerId.load(); }
    void SetClockRate(size_t trackIndex, uint32_t clockRate);
    void Enque(std::unique_ptr<Task> task);
    void Process(std::unique_ptr<Task> task);
    void Process(StartTask* startTask, std::optional<RtpTranslatedPacket> packet);
    bool ReadNextFrame(StartTask* startTask, bool enque);
    bool ClearTasks();
private:
    const uint64_t _mediaId;
    const uint64_t _mediaSourceId;
    const std::weak_ptr<MediaTimer> _timerRef;
    const ProtectedUniquePtr<MediaFrameDeserializer> _deserializer;
    RtpPacketsPlayerStreamCallback* const _callback;
    ProtectedUniquePtr<StartTask> _startTask;
    std::atomic<uint64_t> _timerId = 0;
    // used for adjust of timer interval
    std::atomic<uint32_t> _framesTimeout = 0U;
    ProtectedObj<std::queue<std::unique_ptr<Task>>> _tasks;
};

} // namespace RTC
