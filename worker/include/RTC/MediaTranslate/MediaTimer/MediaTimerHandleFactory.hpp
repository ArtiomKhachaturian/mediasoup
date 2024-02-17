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
    virtual std::unique_ptr<MediaTimerHandle> CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback) = 0;
    virtual void DestroyHandle(std::unique_ptr<MediaTimerHandle> handle);
    static std::unique_ptr<MediaTimerHandleFactory> Create(const std::string& timerName);
protected:
	MediaTimerHandleFactory() = default;
};

} // namespace RTC
