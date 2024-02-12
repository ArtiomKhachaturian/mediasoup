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
    virtual void Start(bool singleshot) = 0;
    virtual void Stop() = 0;
    virtual bool IsStarted() const = 0;
protected:
    MediaTimerHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef);
    std::shared_ptr<MediaTimerCallback> GetCallback() const;
    const std::weak_ptr<MediaTimerCallback>& GetCallbackRef() const;
    bool IsCallbackValid() const;
    virtual void OnTimeoutChanged(uint64_t /*timeoutMs*/) {}
private:
    const std::weak_ptr<MediaTimerCallback> _callbackRef;
    std::atomic<uint64_t> _timeoutMs = 0ULL;
};

} // namespace RTC
