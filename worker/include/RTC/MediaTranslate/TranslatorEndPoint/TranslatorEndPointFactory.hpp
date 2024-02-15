#pragma once
#include <memory>

namespace RTC
{

class TranslatorEndPoint;

class TranslatorEndPointFactory
{
public:
	virtual std::shared_ptr<TranslatorEndPoint> CreateEndPoint() = 0;
protected:
	virtual ~TranslatorEndPointFactory() = default;
};

} // namespace RTC
