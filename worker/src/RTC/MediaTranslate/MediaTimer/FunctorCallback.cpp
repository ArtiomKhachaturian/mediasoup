#include "RTC/MediaTranslate/MediaTimer/FunctorCallback.hpp"

namespace RTC
{

FunctorCallback::FunctorCallback(std::function<void(uint64_t)> onEvent)
    : _onEvent(std::move(onEvent))
{
}

void FunctorCallback::OnEvent(uint64_t timerId)
{ 
	_onEvent(timerId);
}

} // namespace RTC
