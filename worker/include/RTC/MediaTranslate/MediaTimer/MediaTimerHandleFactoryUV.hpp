#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include "RTC/MediaTranslate/ThreadExecution.hpp"

namespace RTC
{

class MediaTimerHandleFactoryUV : public MediaTimerHandleFactory, private ThreadExecution
{
	class Impl;
public:
    ~MediaTimerHandleFactoryUV() final;
    static std::unique_ptr<MediaTimerHandleFactory> Create(const std::string& timerName);
    // impl. of MediaTimerHandleFactory
    std::shared_ptr<MediaTimerHandle> CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback) final;
protected:
    // impl. of ThreadExecution
    void DoExecuteInThread() final;
    void DoStopThread() final;
    void OnSetThreadPriorityError() final;
private:
	MediaTimerHandleFactoryUV(const std::string& timerName, std::shared_ptr<Impl> impl);
private:
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC
