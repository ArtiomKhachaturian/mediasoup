#pragma once

#include "common.hpp"
#include <string>

namespace RTC
{

class ProducerTranslator;

class ProducerTranslatorsManager
{
public:
	virtual ~ProducerTranslatorsManager() = default;
	virtual std::shared_ptr<ProducerTranslator> RegisterProducer(const std::string& producerId) = 0;
    virtual std::shared_ptr<ProducerTranslator> GetRegisteredProducer(const std::string& producerId) const = 0;
    virtual void UnegisterProducer(const std::string& producerId) = 0;
};

} // namespace RTC
