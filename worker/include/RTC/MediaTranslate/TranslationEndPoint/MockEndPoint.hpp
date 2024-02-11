#pragma once
#include "RTC/MediaTranslate/TranslationEndPoint/TranslatorEndPoint.hpp"

namespace RTC
{

class MockEndPoint : public TranslatorEndPoint
{
public:
    MockEndPoint(uint64_t id);
    ~MockEndPoint() final;
protected:
    // impl. of TranslatorEndPoint
    bool IsConnected() const final;
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const MemoryBuffer& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    std::atomic_bool _connected = false;
};

}
