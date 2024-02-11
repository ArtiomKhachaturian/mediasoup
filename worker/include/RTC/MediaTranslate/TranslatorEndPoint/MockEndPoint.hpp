#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include <string_view>

namespace RTC
{

class MockEndPoint : public TranslatorEndPoint
{
    class Impl;
    class TrivialImpl;
    class FileImpl;
public:
    MockEndPoint(uint64_t id);
    MockEndPoint(uint64_t id, const std::string_view& fileNameUtf8,
                 uint32_t intervalBetweenTranslationsMs);
    ~MockEndPoint() final;
protected:
    // impl. of TranslatorEndPoint
    bool IsConnected() const final;
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const MemoryBuffer& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    const std::shared_ptr<Impl> _impl;
};

}
