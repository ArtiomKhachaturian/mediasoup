
#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include <functional>

namespace RTC
{

class FunctorCallback : public MediaTimerCallback
{
public:
    FunctorCallback(std::function<void(uint64_t)> onEvent);
    // impl. of MediaTimerCallback
    void OnEvent(uint64_t timerId) final;
private:
    const std::function<void(uint64_t)> _onEvent;
};

} // namespace RTC
