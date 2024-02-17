#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include <dispatch/dispatch.h>

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
    dispatch_source_t _timer;
    std::atomic_bool _started = false;
};

class MediaTimerHandleFactoryApple : public MediaTimerHandleFactory
{
public:
    MediaTimerHandleFactoryApple(dispatch_queue_t queue);
    ~MediaTimerHandleFactoryApple() final;
    // impl. of MediaTimerHandleFactory
    std::unique_ptr<MediaTimerHandle> CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback) final;
private:
    dispatch_queue_t _queue = nullptr;
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
    dispatch_source_set_event_handler(_timer, nullptr);
    dispatch_source_cancel(_timer);
}

void MediaTimerHandleApple::Start(bool singleshot)
{
    if (!SetStarted(true)) {
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

void MediaTimerHandleApple::OnTimeoutChanged(uint32_t timeoutMs)
{
    MediaTimerHandle::OnTimeoutChanged(timeoutMs);
    SetTimerSourceTimeout(timeoutMs);
}

void MediaTimerHandleApple::SetTimerSourceTimeout(uint32_t timeoutMs)
{
    if (IsStarted()) {
        dispatch_suspend(_timer);
    }
    const auto timeoutNs = NSEC_PER_MSEC * timeoutMs;
    const auto start = dispatch_time(DISPATCH_TIME_NOW, timeoutNs);
    dispatch_source_set_timer(_timer, start, timeoutNs, 0);
    if (IsStarted()) {
        dispatch_resume(_timer);
    }
}

MediaTimerHandleFactoryApple::MediaTimerHandleFactoryApple(dispatch_queue_t queue)
    : _queue(queue)
{
    dispatch_set_target_queue(_queue, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
}

MediaTimerHandleFactoryApple::~MediaTimerHandleFactoryApple()
{
    dispatch_release(_queue);
    _queue = nullptr;
}

std::unique_ptr<MediaTimerHandle> MediaTimerHandleFactoryApple::
    CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback)
{
    if (callback) {
        if (auto timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue)) {
            return std::make_unique<MediaTimerHandleApple>(callback, timer);
        }
    }
    return nullptr;
}

std::unique_ptr<MediaTimerHandleFactory> MediaTimerHandleFactory::Create(const std::string& timerName)
{
    auto queue = dispatch_queue_create(timerName.c_str(), DISPATCH_QUEUE_SERIAL);
    if (queue) {
        return std::make_unique<MediaTimerHandleFactoryApple>(queue);
    }
    return nullptr;
}

} // namespace RTC
