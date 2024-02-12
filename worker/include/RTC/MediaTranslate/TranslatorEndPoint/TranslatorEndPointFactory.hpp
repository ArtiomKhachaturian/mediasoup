#pragma once
#include <memory>

namespace RTC
{

class TranslatorEndPoint;

class TranslatorEndPointFactory
{
public:
	virtual std::shared_ptr<TranslatorEndPoint> CreateEndPoint(uint32_t ssrc) = 0;
protected:
	virtual ~TranslatorEndPointFactory() = default;
};

} // namespace RTC
