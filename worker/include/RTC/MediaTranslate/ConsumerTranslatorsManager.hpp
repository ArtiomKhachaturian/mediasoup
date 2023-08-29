#pragma once

#include "common.hpp"
#include <string>

namespace RTC
{

class ConsumerTranslator;

class ConsumerTranslatorsManager
{
public:
	virtual ~ConsumerTranslatorsManager() = default;
	virtual std::shared_ptr<ConsumerTranslator> RegisterConsumer(const std::string& consumerId) = 0;
    virtual std::shared_ptr<ConsumerTranslator> GetRegisteredConsumer(const std::string& consumerId) const = 0;
    virtual void UnegisterConsumer(const std::string& consumerId) = 0;
};


} // namespace RTC
