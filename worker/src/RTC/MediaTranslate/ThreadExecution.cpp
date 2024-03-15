#include "RTC/MediaTranslate/ThreadExecution.hpp"
#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#ifndef __APPLE__
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif
#include <algorithm> // for std::max
#endif

namespace RTC
{

enum class ThreadExecution::State
{
    Initial,
    Started,
    Stopped
};

ThreadExecution::ThreadExecution(std::string threadName, ThreadPriority priority)
    : _threadName(std::move(threadName))
    , _priority(priority)
    , _state(State::Initial)
{
}

bool ThreadExecution::StartExecution(bool waitingUntilNotStarted)
{
    if (State::Started != _state.exchange(State::Started)) {
        {
            LOCK_WRITE_PROTECTED_OBJ(_thread);
            _thread = std::thread(std::bind(&ThreadExecution::Execute, this));
        }
        if (waitingUntilNotStarted) {
            while (!IsActive()) {
                std::this_thread::yield();
            }
        }
        return true;
    }
    return false;
}

bool ThreadExecution::StopExecution()
{
    State expected = State::Started;
    if (_state.compare_exchange_strong(expected, State::Stopped)) {
        DoStopThread();
        LOCK_WRITE_PROTECTED_OBJ(_thread);
        if (_thread->joinable()) {
            _thread->join();
        }
        _thread = std::thread();
        return true;
    }
    return false;
}

bool ThreadExecution::IsActive() const
{
    LOCK_READ_PROTECTED_OBJ(_thread);
    return _thread->joinable();
}

bool ThreadExecution::IsCancelled() const
{
    return State::Stopped == _state.load();
}

void ThreadExecution::Execute()
{
    if (!IsCancelled()) {
        if (!SetCurrentThreadPriority()) {
            OnSetThreadPriorityError();
        }
        if (!SetCurrentThreadName()) {
            OnSetThreadNameError();
        }
        DoExecuteInThread();
    }
}

bool ThreadExecution::SetCurrentThreadName() const
{
    if (!_threadName.empty()) {
#ifdef WIN32
        std::string_view nameView(_threadName);
        // Win32 has limitation for thread name - max 63 symbols
        if (nameView.size() > 62U) {
            nameView = nameView.substr(0, 62U);
        }
        struct
        {
            DWORD dwType;
            LPCSTR szName;
            DWORD dwThreadID;
            DWORD dwFlags;
        } threadname_info = {0x1000, name.data(), static_cast<DWORD>(-1), 0};

        __try {
            ::RaiseException(0x406D1388, 0, sizeof(threadname_info) / sizeof(DWORD), reinterpret_cast<ULONG_PTR*>(&threadname_info));
        } __except (EXCEPTION_EXECUTE_HANDLER) { /* NOLINT */ }
        return true;
#elif defined(__APPLE__)
        return 0 == pthread_setname_np(_threadName.data());
#else
        return 0 == prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(_threadName.data())); // NOLINT
#endif
    }
    return true; // ok if name is not specified or empty
}

bool ThreadExecution::SetCurrentThreadPriority() const
{
    if (ThreadPriority::Auto != _priority) {
#ifdef WIN32
        return TRUE == ::SetThreadPriority(::GetCurrentThread(), static_cast<int>(_priority));
#else
        if (const auto thread = pthread_self()) {
            const int policy = SCHED_FIFO;
            const int min = sched_get_priority_min(policy);
            const int max = sched_get_priority_max(policy);
            if (-1 != min && -1 != max && max - min > 2) {
                // convert ThreadPriority priority to POSIX priorities:
                sched_param param;
                const int top = max - 1;
                const int low = min + 1;
                switch (_priority) {
                    case ThreadPriority::Low:
                        param.sched_priority = low;
                        break;
                    case ThreadPriority::Normal:
                        // the -1 ensures that the High is always greater or equal to Normal
                        param.sched_priority = (low + top - 1) / 2;
                        break;
                    case ThreadPriority::High:
                        param.sched_priority = std::max(top - 2, low);
                        break;
                    case ThreadPriority::Highest:
                        param.sched_priority = std::max(top - 1, low);
                        break;
                    case ThreadPriority::Realtime:
                        param.sched_priority = top;
                        break;
                    default:
                        break;
                }
                return 0 == pthread_setschedparam(thread, policy, &param);
            }
        }
        return false;
#endif
    }
    return true; // auto
}

} // namespace RTC
