#pragma once
#include <memory>
#include <string>

namespace RTC
{

class MediaTimerHandle;
class MediaTimerCallback;

class MediaTimerHandleFactory
{
public:
    virtual ~MediaTimerHandleFactory() = default;
    virtual MediaTimerHandle* CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback) = 0;
    virtual void DestroyHandle(MediaTimerHandle* handle);
protected:
	MediaTimerHandleFactory() = default;
};

} // namespace RTC
