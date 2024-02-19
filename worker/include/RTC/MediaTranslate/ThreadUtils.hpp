#pragma once
#include "RTC/MediaTranslate/ThreadPriority.hpp"
#include <string_view>

namespace RTC
{

bool SetCurrentThreadName(std::string_view name);

bool SetCurrentThreadPriority(ThreadPriority priority);

} // namespace RTC
