#pragma once
#include <memory>

namespace RTC
{

class MediaTimerHandle;
class MediaTimerCallback;

class MediaTimerHandleFactory
{
public:
    virtual ~MediaTimerHandleFactory() = default;
    virtual std::unique_ptr<MediaTimerHandle> CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef) = 0;
    static std::unique_ptr<MediaTimerHandleFactory> Create();
protected:
	MediaTimerHandleFactory() = default;
};

} // namespace RTC
