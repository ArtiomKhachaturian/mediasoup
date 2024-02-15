#pragma once
#include <atomic>
#include <memory>

namespace RTC
{

class MediaTimerCallback;

class MediaTimerHandle
{
public:
    virtual ~MediaTimerHandle() = default;
    uint64_t GetTimeout() const { return _timeoutMs.load(); }
    void SetTimeout(uint64_t timeoutMs);
    const auto& GetCallback() const { return _callback; }
    virtual void Start(bool singleshot) = 0;
    virtual void Stop() = 0;
    virtual bool IsStarted() const = 0;
protected:
    MediaTimerHandle(const std::shared_ptr<MediaTimerCallback>& callback);
    virtual void OnTimeoutChanged(uint64_t /*timeoutMs*/) {}
private:
    const std::shared_ptr<MediaTimerCallback> _callback;
    std::atomic<uint64_t> _timeoutMs = 0ULL;
};

} // namespace RTC
