#pragma once
#include <cstdint>

namespace RTC
{

class MediaTimerCallback
{
public:
    virtual void OnCallbackRegistered(uint64_t /*timerId*/, bool /*registered*/) {}
    virtual void OnEvent(uint64_t timerId) = 0;
protected:
    virtual ~MediaTimerCallback() = default;
};

} // namespace RTC
