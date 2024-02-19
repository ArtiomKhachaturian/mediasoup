#include "RTC/MediaTranslate/MediaTimer/FunctorCallback.hpp"

namespace RTC
{

FunctorCallback::FunctorCallback(std::function<void(void)> onEvent)
    : _onEvent(std::move(onEvent))
{
}

void FunctorCallback::OnEvent() 
{ 
	_onEvent(); 
}

} // namespace RTC