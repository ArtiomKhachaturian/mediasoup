#pragma once
#include <limits>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace RTC
{

enum class ThreadPriority : int
{
    Auto     = std::numeric_limits<int>::min(),
#ifdef _WIN32
    Low      = THREAD_PRIORITY_BELOW_NORMAL,
    Normal   = THREAD_PRIORITY_NORMAL,
    High     = THREAD_PRIORITY_ABOVE_NORMAL,
    Highest  = THREAD_PRIORITY_HIGHEST,
    Realtime = THREAD_PRIORITY_TIME_CRITICAL
#else
    Low      = 1,
    Normal   = 2,
    High     = 3,
    Highest  = 4,
    Realtime = 5
#endif
};

const char* ToString(ThreadPriority state);

} // namespace RTC
