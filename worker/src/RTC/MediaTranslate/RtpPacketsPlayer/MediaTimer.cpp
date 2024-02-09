#include "RTC/MediaTranslate/RtpPacketsPlayer/MediaTimer.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/MediaTimerCallback.hpp"
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif
#include <atomic>

namespace RTC
{

class MediaTimer::PlatformHandle
{
    friend class PlatformHandleFactory;
public:
    PlatformHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef);
    ~PlatformHandle();
    std::shared_ptr<MediaTimerCallback> GetCallback() const { return _callbackRef.lock(); }
    void SetTimeout(uint64_t timeoutMs);
    void Start(bool singleshot);
    void Stop();
private:
    void OnTimeoutChanged(uint64_t timeoutMs);
    void SetTimer(dispatch_source_t timer);
private:
    const std::weak_ptr<MediaTimerCallback> _callbackRef;
    std::atomic<uint64_t> _timeoutMs = 0ULL;
#ifdef __APPLE__
    std::atomic_bool _running = false; // under protection of _timer
    std::atomic<dispatch_source_t> _timer = nullptr;
#endif
};

class MediaTimer::PlatformHandleFactory
{
public:
    PlatformHandleFactory() = default;
    ~PlatformHandleFactory();
    std::unique_ptr<PlatformHandle> CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef);
    static std::unique_ptr<PlatformHandleFactory> Create();
private:
#ifdef __APPLE__
    dispatch_queue_t _queue = nullptr;
#endif
};

MediaTimer::MediaTimer()
    : _factory(PlatformHandleFactory::Create())
{
}

MediaTimer::~MediaTimer()
{
    LOCK_WRITE_PROTECTED_OBJ(_handles);
    _handles->clear();
}

uint64_t MediaTimer::RegisterTimer(const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    if (!callbackRef.expired() && _factory) {
        if (auto handle = _factory->CreateHandle(callbackRef)) {
            const auto timerId = reinterpret_cast<uint64_t>(handle.get());
            {
                LOCK_WRITE_PROTECTED_OBJ(_handles);
                _handles.Ref()[timerId] = std::move(handle);
            }
            return timerId;
        }
    }
    return 0UL;
}

void MediaTimer::UnregisterTimer(uint64_t timerId)
{
    if (timerId) {
        LOCK_WRITE_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            _handles->erase(it);
        }
    }
}

void MediaTimer::SetTimeout(uint64_t timerId, uint64_t timeoutMs)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->SetTimeout(timeoutMs);
        }
    }
}

void MediaTimer::Start(uint64_t timerId, bool singleshot)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Start(singleshot);
        }
    }
}

void MediaTimer::Stop(uint64_t timerId)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Stop();
        }
    }
}

MediaTimer::PlatformHandle::PlatformHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef)
    : _callbackRef(callbackRef)
{
}

MediaTimer::PlatformHandle::~PlatformHandle()
{
    Stop();
#ifdef __APPLE__
    SetTimer(nullptr);
#endif
}

void MediaTimer::PlatformHandle::SetTimeout(uint64_t timeoutMs)
{
    if (timeoutMs != _timeoutMs.exchange(timeoutMs)) {
        OnTimeoutChanged(timeoutMs);
    }
}

#ifdef __APPLE__
void MediaTimer::PlatformHandle::OnTimeoutChanged(uint64_t timeoutMs)
{
    if (const auto timer = _timer.load()) {
        if (_running.load()) {
            dispatch_suspend(timer);
        }
        dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, NSEC_PER_MSEC * timeoutMs, 0);
        if (_running.load()) {
            dispatch_resume(timer);
        }
    }
}

void MediaTimer::PlatformHandle::SetTimer(dispatch_source_t timer)
{
    if (const auto oldTimer = _timer.exchange(timer)) {
        dispatch_source_set_event_handler(oldTimer, nullptr);
        dispatch_source_cancel(oldTimer);
        _running = false;
    }
    if (timer) {
        OnTimeoutChanged(_timeoutMs.load());
    }
}

void MediaTimer::PlatformHandle::Start(bool singleshot)
{
    if (const auto timer = _timer.load()) {
        if (!_running.exchange(true)) {
            dispatch_source_set_event_handler(timer, ^{
                if (singleshot) {
                    Stop();
                }
                if (const auto callback = GetCallback()) {
                    callback->OnEvent();
                }
            });
            dispatch_resume(timer);
        }
    }
}

void MediaTimer::PlatformHandle::Stop()
{
    if (_running.exchange(false)) {
        if (const auto timer = _timer.load()) {
            dispatch_suspend(timer);
        }
    }
}

MediaTimer::PlatformHandleFactory::~PlatformHandleFactory()
{
    _queue = nullptr;
}

std::unique_ptr<MediaTimer::PlatformHandle> MediaTimer::PlatformHandleFactory::
    CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    if (!callbackRef.expired()) {
        auto handle = std::make_unique<PlatformHandle>(callbackRef);
        if (auto timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue)) {
            handle->SetTimer(timer);
            return handle;
        }
    }
    return nullptr;
}

std::unique_ptr<MediaTimer::PlatformHandleFactory> MediaTimer::PlatformHandleFactory::Create()
{
    dispatch_queue_t queue = dispatch_queue_create("MediaTimer", DISPATCH_QUEUE_SERIAL);
    if (queue) {
        auto factory = std::make_unique<PlatformHandleFactory>();
        factory->_queue = queue;
        return factory;
    }
    return nullptr;
}
#endif

} // namespace RTC
