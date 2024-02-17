#pragma once
#include <string>
#include <memory>
#include <functional>

namespace RTC
{

class MediaTimerCallback;

class MediaTimer
{
    class Impl;
public:
    MediaTimer(std::string timerName = std::string());
    MediaTimer(const MediaTimer&) = delete;
    MediaTimer(MediaTimer&&) = delete;
    ~MediaTimer();
    MediaTimer& operator = (const MediaTimer&) = delete;
    MediaTimer& operator = (MediaTimer&&) = delete;
    uint64_t RegisterTimer(const std::shared_ptr<MediaTimerCallback>& callback);
    uint64_t RegisterTimer(std::function<void(void)> onEvent);
    void UnregisterTimer(uint64_t timerId);
    // time-out in milliseconds, previous invokes will discarded
    void SetTimeout(uint64_t timerId, uint32_t timeoutMs);
    void Start(uint64_t timerId, bool singleshot);
    void Stop(uint64_t timerId);
    bool IsStarted(uint64_t timerId) const;
    // returns timer ID
    uint64_t Singleshot(uint32_t afterMs, const std::shared_ptr<MediaTimerCallback>& callback);
    uint64_t Singleshot(uint32_t afterMs, std::function<void(void)> onEvent);
private:
    static std::shared_ptr<MediaTimerCallback> CreateCallback(std::function<void(void)> onEvent);
private:
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC
