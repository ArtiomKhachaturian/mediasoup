#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include <time.h>

namespace RTC
{

class MediaTimerHandleUnix : public MediaTimerHandle
{
public:
    MediaTimerHandleUnix(const std::weak_ptr<MediaTimerCallback>& callbackRef);
    ~MediaTimerHandleUnix() final;
    // impl. of MediaTimerHandle
    void Start(bool singleshot) final;
    void Stop() final;
protected:
    // overrides of MediaTimerHandle
    void OnTimeoutChanged(uint64_t timeoutMs) final;
};

class MediaTimerHandleFactoryUnix : public MediaTimerHandleFactory
{
public:
    MediaTimerHandleFactoryUnix();
    ~MediaTimerHandleFactoryUnix() final;
    // impl. of MediaTimerHandleFactory
    std::unique_ptr<MediaTimerHandle> CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef) final;
};


MediaTimerHandleUnix::MediaTimerHandleUnix(const std::weak_ptr<MediaTimerCallback>& callbackRef)
    : MediaTimerHandle(callbackRef)
{
}

MediaTimerHandleUnix::~MediaTimerHandleUnix()
{
    MediaTimerHandleUnix::Stop();
}

void MediaTimerHandleUnix::Start(bool singleshot)
{
    
}

void MediaTimerHandleUnix::Stop()
{
    
}

void MediaTimerHandleUnix::OnTimeoutChanged(uint64_t timeoutMs)
{
    MediaTimerHandle::OnTimeoutChanged(timeoutMs);
}

MediaTimerHandleFactoryUnix::MediaTimerHandleFactoryUnix()
{
}

MediaTimerHandleFactoryUnix::~MediaTimerHandleFactoryUnix()
{
}

std::unique_ptr<MediaTimerHandle> MediaTimerHandleFactoryUnix::
    CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    return nullptr;
}

std::unique_ptr<MediaTimerHandleFactory> MediaTimerHandleFactory::Create(const std::string& timerName)
{
    return std::make_unique<MediaTimerHandleFactoryUnix>();
}

} // namespace RTC
