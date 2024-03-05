#pragma once
#include "RTC/ObjectId.hpp"
#include <atomic>
#include <memory>

namespace RTC
{

class MediaTimerCallback;

class MediaTimerHandle : public ObjectId
{
public:
    virtual ~MediaTimerHandle();
    uint32_t GetTimeout() const { return _timeoutMs.load(); }
    void SetTimeout(uint32_t timeoutMs);
    virtual void Start(bool singleshot) = 0;
    virtual void Stop() = 0;
    virtual bool IsStarted() const = 0;
protected:
    MediaTimerHandle(const std::shared_ptr<MediaTimerCallback>& callback);
    virtual void OnTimeoutChanged(uint32_t /*timeoutMs*/) {}
private:
    const std::shared_ptr<MediaTimerCallback> _callback;
    std::atomic<uint32_t> _timeoutMs = 0ULL;
};

} // namespace RTC
