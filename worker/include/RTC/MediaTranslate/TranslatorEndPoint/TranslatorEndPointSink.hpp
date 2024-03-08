#pragma once
#include "RTC/MediaTranslate/MediaSink.hpp"

namespace RTC
{	

class TranslatorEndPointSink : public MediaSink
{
public:
	virtual void NotifyThatConnectionEstablished(uint64_t /*endPointId*/, bool /*connected*/) {}
};

} // namespace RTC
