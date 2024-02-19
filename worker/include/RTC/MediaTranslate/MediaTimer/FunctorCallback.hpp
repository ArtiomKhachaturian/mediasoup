
#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include <functional>

namespace RTC
{

class FunctorCallback : public MediaTimerCallback
{
public:
    FunctorCallback(std::function<void(void)> onEvent);
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    const std::function<void(void)> _onEvent;
};

} // namespace RTC