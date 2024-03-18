#pragma once
#include "RTC/MediaTranslate/ThreadPriority.hpp"
#include "ProtectedObj.hpp"
#include <atomic>
#include <string>
#include <thread>

namespace RTC
{

class ThreadExecution
{
    enum class State;
public:
    virtual ~ThreadExecution() { StopExecution(); }
    const std::string& GetThreadName() const { return _threadName; }
    ThreadPriority GetPriority() const { return _priority; }
    // returns true if state was changed
    bool StartExecution(bool waitingUntilNotStarted = false);
    // don't forget to call this method before destroy of derived class instance
    bool StopExecution();
    bool IsActive() const;
protected:
    ThreadExecution(std::string threadName = std::string(),
                    ThreadPriority priority = ThreadPriority::High);
    bool IsCancelled() const;
    // called inside of thread routine, after start
    virtual void DoExecuteInThread() = 0;
    // called after changes of internal state to 'stopped' but before joining of execution thread
    virtual void DoStopThread() {}
    // called if setup ot thread priority or name was failed,
    // can be ignored because it's not critical issues
    virtual void OnSetThreadPriorityError() {}
    virtual void OnSetThreadNameError() {}
private:
    ThreadExecution(const ThreadExecution&) = delete;
    ThreadExecution(ThreadExecution&&) = delete;
    void Execute();
    bool SetCurrentThreadName() const;
    bool SetCurrentThreadPriority() const;
private:
    const std::string _threadName;
    const ThreadPriority _priority;
    ProtectedObj<std::thread, std::mutex> _thread;
    std::atomic<State> _state;
};

} // namespace RTC
