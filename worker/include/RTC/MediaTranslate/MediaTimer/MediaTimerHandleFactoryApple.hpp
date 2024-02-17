#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include <dispatch/dispatch.h>

namespace RTC
{

// not recommended, because SFU is not fully compatible with Apple eco-system
// and doesn't have NSApplication for correct management of GCD queue objects
class MediaTimerHandleFactoryApple : public MediaTimerHandleFactory
{
public:
    ~MediaTimerHandleFactoryApple() final;
    static std::unique_ptr<MediaTimerHandleFactory> Create(const std::string& timerName);
    // impl. of MediaTimerHandleFactory
    std::unique_ptr<MediaTimerHandle> CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback) final;
private:
	MediaTimerHandleFactoryApple(dispatch_queue_t queue);
private:
    const dispatch_queue_t _queue;
};

} // namespace RTC
