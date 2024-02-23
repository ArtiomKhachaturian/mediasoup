#pragma once
#include "RTC/MediaTranslate/MediaSink.hpp"

namespace RTC
{	

class TranslatorEndPointSink : public MediaSink
{
public:
	virtual void NotifyThatConnectionEstablished(const MediaObject& /*endPoint*/, bool /*connected*/) {}
};

} // namespace RTC