#pragma once
#include <string>
#include <memory>
#include <optional>
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
    uint64_t Register(const std::shared_ptr<MediaTimerCallback>& callback);
    uint64_t Register(std::function<void(uint64_t)> onEvent);
    uint64_t RegisterAndStart(const std::shared_ptr<MediaTimerCallback>& callback,
                              uint32_t timeoutMs, bool singleshot = false);
    uint64_t RegisterAndStart(std::function<void(uint64_t)> onEvent,
                              uint32_t timeoutMs, bool singleshot = false);
    void Unregister(uint64_t timerId);
    // time-out in milliseconds, previous invokes will discarded
    void SetTimeout(uint64_t timerId, uint32_t timeoutMs);
    void Start(uint64_t timerId, bool singleshot = false);
    void Stop(uint64_t timerId);
    bool IsStarted(uint64_t timerId) const;
    std::optional<uint32_t> GetTimeout(uint64_t timerId) const;
    // returns timer ID
    uint64_t Singleshot(uint32_t afterMs, const std::shared_ptr<MediaTimerCallback>& callback);
    uint64_t Singleshot(uint32_t afterMs, std::function<void(void)> onEvent);
private:
    static std::shared_ptr<MediaTimerCallback> CreateCallback(std::function<void(uint64_t)> onEvent);
private:
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC
