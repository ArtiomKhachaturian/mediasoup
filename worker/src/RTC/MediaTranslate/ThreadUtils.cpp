#include "RTC/MediaTranslate/ThreadUtils.hpp"
#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#include <algorithm> // for std::max
#endif

namespace RTC
{

bool SetCurrentThreadName(std::string_view name)
{
    if (name.data()) {
#ifdef WIN32
        // Win32 has limitation for thread name - max 63 symbols
        if (name.size() > 62U) {
            name = name.substr(0, 62U);
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
        return 0 == pthread_setname_np(name.data());
#else
        return 0 == prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(name.data())); // NOLINT
#endif
    }
    return false;
}

bool SetCurrentThreadPriority(ThreadPriority priority)
{
#ifdef WIN32
    return TRUE == ::SetThreadPriority(::GetCurrentThread(), static_cast<int>(priority));
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
            switch (priority) {
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
            }
            return 0 == pthread_setschedparam(thread, policy, &param);
        }
    }
    return false;
#endif
}

} // namespace RTC
