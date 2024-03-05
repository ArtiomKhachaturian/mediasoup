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
class RtpPacketsPlayerCallback;
class RtpCodecMimeType;

class RtpPacketsPlayerMediaFragmentQueue : public MediaTimerCallback
{
    class Task;
    class StartTask;
    class MediaFrameTask;
public:
    static std::shared_ptr<RtpPacketsPlayerMediaFragmentQueue> Create(const std::weak_ptr<MediaTimer>& timerRef,
                                                                      std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                                      RtpPacketsPlayerCallback* callback);
    ~RtpPacketsPlayerMediaFragmentQueue() final;
    void SetTimerId(uint64_t timerId);
    void Start(size_t trackIndex, uint32_t ssrc, uint32_t clockRate,
               uint64_t mediaId, uint64_t mediaSourceId);
    size_t GetTracksCount() const;
    std::optional<RtpCodecMimeType> GetTrackType(size_t trackIndex) const;
    // impl. of MediaTimerCallback
    void OnEvent(uint64_t timerId) final;
private:
    RtpPacketsPlayerMediaFragmentQueue(const std::weak_ptr<MediaTimer>& timerRef,
                                       std::unique_ptr<MediaFrameDeserializer> deserializer,
                                       RtpPacketsPlayerCallback* callback);
    uint64_t GetTimerId() const { return _timerId.load(); }
    void SetClockRate(size_t trackIndex, uint32_t clockRate);
    void Enque(std::unique_ptr<Task> task);
    void Process(std::unique_ptr<Task> task);
    void Process(StartTask* startTask, std::optional<RtpTranslatedPacket> packet);
    bool ReadNextFrame(StartTask* startTask, bool enque);
    bool ClearTasks();
private:
    const std::weak_ptr<MediaTimer> _timerRef;
    const ProtectedUniquePtr<MediaFrameDeserializer> _deserializer;
    RtpPacketsPlayerCallback* const _callback;
    ProtectedUniquePtr<StartTask> _startTask;
    std::atomic<uint64_t> _timerId = 0;
    // used for adjust of timer interval
    ProtectedObj<std::queue<std::unique_ptr<Task>>> _tasks;
};

} // namespace RTC
