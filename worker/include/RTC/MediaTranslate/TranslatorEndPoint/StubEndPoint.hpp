#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include <atomic>

namespace RTC
{

class StubEndPoint : public TranslatorEndPoint
{
public:
    StubEndPoint(uint32_t ssrc);
    ~StubEndPoint() final;
protected:
    // impl. of TranslatorEndPoint
    bool IsConnected() const final { return _connected.load(); }
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const MemoryBuffer& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    std::atomic_bool _connected = false;
};

} // namespace RTC
