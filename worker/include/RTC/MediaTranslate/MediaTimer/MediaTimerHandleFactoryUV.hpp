#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include <atomic>
#include <thread>

namespace RTC
{

class MediaTimerHandleFactoryUV : public MediaTimerHandleFactory
{
	class Impl;
public:
    ~MediaTimerHandleFactoryUV() final;
    static std::unique_ptr<MediaTimerHandleFactory> Create(const std::string& timerName);
    // impl. of MediaTimerHandleFactory
    std::unique_ptr<MediaTimerHandle> CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback) final;
private:
	MediaTimerHandleFactoryUV(const std::string& timerName, std::shared_ptr<Impl> impl);
    void Run();
    bool IsCancelled() const { return _cancelled.load(); }
    static void SetThreadName(const std::string& timerName);
private:
    const std::string _timerName;
    const std::shared_ptr<Impl> _impl;
    std::thread _thread;
    std::atomic_bool _cancelled = false;
};

} // namespace RTC