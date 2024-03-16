#pragma once
#include <cstdint>

namespace RTC
{

class MediaTimerCallback
{
public:
    virtual ~MediaTimerCallback() = default;
    virtual void OnCallbackRegistered(uint64_t /*timerId*/, bool /*registered*/) {}
    virtual void OnEvent(uint64_t timerId) = 0;
};

} // namespace RTC
