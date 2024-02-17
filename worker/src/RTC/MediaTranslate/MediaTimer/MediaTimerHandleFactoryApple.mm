#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactoryApple.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "ProtectedObj.hpp"
#if ! __has_feature(objc_arc)
    #error "ARC is off"
#endif

namespace RTC
{

class MediaTimerHandleApple : public MediaTimerHandle
{
public:
    MediaTimerHandleApple(const std::shared_ptr<MediaTimerCallback>& callback,
                          dispatch_source_t timer);
    ~MediaTimerHandleApple() final;
    // impl. of MediaTimerHandle
    void Start(bool singleshot) final;
    void Stop() final;
    bool IsStarted() const { return _started.load(); }
protected:
    // overrides of MediaTimerHandle
    void OnTimeoutChanged(uint32_t timeoutMs) final;
private:
    bool SetStarted(bool started) { return _started.exchange(started); }
    void SetTimerSourceTimeout(uint32_t timeoutMs);
private:
    ProtectedObj<dispatch_source_t> _timer;
    std::atomic_bool _started = false;
};


MediaTimerHandleApple::MediaTimerHandleApple(const std::shared_ptr<MediaTimerCallback>& callback,
                                             dispatch_source_t timer)
    : MediaTimerHandle(callback)
    , _timer(timer)
{
    SetTimerSourceTimeout(GetTimeout());
}

MediaTimerHandleApple::~MediaTimerHandleApple()
{
    MediaTimerHandleApple::Stop();
    @autoreleasepool {
        LOCK_WRITE_PROTECTED_OBJ(_timer);
        if (auto timer = _timer.Exchange(nil)) {
            dispatch_source_set_event_handler(timer, nullptr);
            // dealloc will happen here after everything is done cancelling
            __block dispatch_source_t cancelledTimer = timer;
            dispatch_source_set_cancel_handler(timer, ^{ cancelledTimer = nil; });
            dispatch_source_cancel(timer);
            // works fine since this doesn't actually dealloc now
            timer = nil;
        }
    }
}

void MediaTimerHandleApple::Start(bool singleshot)
{
    if (!SetStarted(true)) {
        @autoreleasepool {
            LOCK_READ_PROTECTED_OBJ(_timer);
            if (const auto timer = _timer.ConstRef()) {
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
}

void MediaTimerHandleApple::Stop()
{
    if (SetStarted(false)) {
        @autoreleasepool {
            LOCK_READ_PROTECTED_OBJ(_timer);
            if (const auto timer = _timer.ConstRef()) {
                dispatch_suspend(timer);
            }
        }
    }
}

void MediaTimerHandleApple::OnTimeoutChanged(uint32_t timeoutMs)
{
    MediaTimerHandle::OnTimeoutChanged(timeoutMs);
    SetTimerSourceTimeout(timeoutMs);
}

void MediaTimerHandleApple::SetTimerSourceTimeout(uint32_t timeoutMs)
{
    @autoreleasepool {
        LOCK_READ_PROTECTED_OBJ(_timer);
        if (const auto timer = _timer.ConstRef()) {
            const auto timeoutNs = NSEC_PER_MSEC * timeoutMs;
            const auto start = dispatch_time(DISPATCH_TIME_NOW, timeoutNs);
            const auto wasStarted = IsStarted();
            if (wasStarted) {
                dispatch_suspend(timer);
            }
            dispatch_source_set_timer(timer, start, timeoutNs, 0);
            if (wasStarted) {
                dispatch_resume(timer);
            }
        }
    }
}

MediaTimerHandleFactoryApple::MediaTimerHandleFactoryApple(dispatch_queue_t queue)
    : _queue(queue)
{
    dispatch_set_target_queue(_queue, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
}

MediaTimerHandleFactoryApple::~MediaTimerHandleFactoryApple()
{
}

std::unique_ptr<MediaTimerHandleFactory> MediaTimerHandleFactoryApple::Create(const std::string& timerName)
{
    std::unique_ptr<MediaTimerHandleFactory> factory;
    /*@autoreleasepool {
        if (auto queue = dispatch_queue_create(timerName.c_str(), DISPATCH_QUEUE_SERIAL)) {
            factory.reset(new MediaTimerHandleFactoryApple(queue));
        }
    }*/
    return factory;
}

std::unique_ptr<MediaTimerHandle> MediaTimerHandleFactoryApple::
    CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback)
{
    if (callback) {
        @autoreleasepool {
            if (auto timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue)) {
                return std::make_unique<MediaTimerHandleApple>(callback, timer);
            }
        }
    }
    return nullptr;
}

} // namespace RTC
