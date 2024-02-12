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
    MockEndPoint(uint32_t ssrc);
    MockEndPoint(uint32_t ssrc, const std::string_view& fileNameUtf8,
                 uint32_t intervalBetweenTranslationsMs);
    MockEndPoint(const std::string_view& fileNameUtf8, uint32_t intervalBetweenTranslationsMs);
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
