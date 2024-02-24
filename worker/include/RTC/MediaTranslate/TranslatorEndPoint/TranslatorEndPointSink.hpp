#pragma once
#include "RTC/MediaTranslate/MediaSink.hpp"

namespace RTC
{	

class TranslatorEndPointSink : public MediaSink
{
public:
	virtual void NotifyThatConnectionEstablished(const ObjectId& /*endPoint*/,
                                                 bool /*connected*/) {}
};

} // namespace RTC
