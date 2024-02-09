#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include <dispatch/dispatch.h>

namespace RTC
{

class MediaTimerHandleApple : public MediaTimerHandle
{
public:
    MediaTimerHandleApple(const std::weak_ptr<MediaTimerCallback>& callbackRef,
                          dispatch_source_t timer);
    ~MediaTimerHandleApple() final;
    // impl. of MediaTimerHandle
    void Start(bool singleshot) final;
    void Stop() final;
protected:
    // overrides of MediaTimerHandle
    void OnTimeoutChanged(uint64_t timeoutMs) final;
private:
    bool IsStarted() const { return _started.load(); }
    bool SetStarted(bool started) { return _started.exchange(started); }
    void SetTimerSourceTimeout(uint64_t timeoutMs);
private:
    dispatch_source_t _timer;
    std::atomic_bool _started = false;
};

class MediaTimerHandleFactoryApple : public MediaTimerHandleFactory
{
public:
    MediaTimerHandleFactoryApple(dispatch_queue_t queue);
    ~MediaTimerHandleFactoryApple() final;
    // impl. of MediaTimerHandleFactory
    std::unique_ptr<MediaTimerHandle> CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef) final;
private:
    dispatch_queue_t _queue = nullptr;
};


MediaTimerHandleApple::MediaTimerHandleApple(const std::weak_ptr<MediaTimerCallback>& callbackRef,
                                             dispatch_source_t timer)
    : MediaTimerHandle(callbackRef)
    , _timer(timer)
{
    dispatch_retain(_timer);
    SetTimerSourceTimeout(0ULL);
}

MediaTimerHandleApple::~MediaTimerHandleApple()
{
    MediaTimerHandleApple::Stop();
    dispatch_source_set_event_handler(_timer, nullptr);
    dispatch_source_cancel(_timer);
    dispatch_release(_timer);
    _timer = nullptr;
}

void MediaTimerHandleApple::Start(bool singleshot)
{
    if (IsCallbackValid() && !SetStarted(true)) {
        dispatch_source_set_event_handler(_timer, ^{
            if (singleshot) {
                Stop();
            }
            if (const auto callback = GetCallback()) {
                callback->OnEvent();
            }
        });
        dispatch_resume(_timer);
    }
}

void MediaTimerHandleApple::Stop()
{
    if (SetStarted(false)) {
        dispatch_suspend(_timer);
    }
}

void MediaTimerHandleApple::OnTimeoutChanged(uint64_t timeoutMs)
{
    MediaTimerHandle::OnTimeoutChanged(timeoutMs);
    SetTimerSourceTimeout(timeoutMs);
}

void MediaTimerHandleApple::SetTimerSourceTimeout(uint64_t timeoutMs)
{
    if (IsStarted()) {
        dispatch_suspend(_timer);
    }
    dispatch_source_set_timer(_timer, DISPATCH_TIME_NOW, NSEC_PER_MSEC * timeoutMs, 0);
    if (IsStarted()) {
        dispatch_resume(_timer);
    }
}

MediaTimerHandleFactoryApple::MediaTimerHandleFactoryApple(dispatch_queue_t queue)
    : _queue(queue)
{
    dispatch_retain(_queue);
}

MediaTimerHandleFactoryApple::~MediaTimerHandleFactoryApple()
{
    dispatch_release(_queue);
    _queue = nullptr;
}

std::unique_ptr<MediaTimerHandle> MediaTimerHandleFactoryApple::
    CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    if (!callbackRef.expired()) {
        if (auto timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue)) {
            return std::make_unique<MediaTimerHandleApple>(callbackRef, timer);
        }
    }
    return nullptr;
}

std::unique_ptr<MediaTimerHandleFactory> MediaTimerHandleFactory::Create()
{
    auto queue = dispatch_queue_create("MediaTimer", DISPATCH_QUEUE_SERIAL);
    if (queue) {
        return std::make_unique<MediaTimerHandleFactoryApple>(queue);
    }
    return nullptr;
}

} // namespace RTC
