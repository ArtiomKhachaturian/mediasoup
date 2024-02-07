#pragma once
#include <string>

namespace RTC
{

class TranslatorUnit
{
public:
    virtual ~TranslatorUnit() = default;
    virtual const std::string& GetId() const = 0;
    virtual const std::string& GetLanguageId() const = 0;
};

}
