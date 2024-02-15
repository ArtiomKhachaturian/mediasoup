#include "RTC/MediaTranslate/TranslatorEndPoint/StubEndPoint.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"

namespace RTC
{

StubEndPoint::~StubEndPoint()
{
    StubEndPoint::Disconnect();
}

void StubEndPoint::Connect()
{
    if (!_connected.exchange(true)) {
        NotifyThatConnectionEstablished(true);
    }
}

void StubEndPoint::Disconnect()
{
    if (_connected.exchange(false)) {
        NotifyThatConnectionEstablished(false);
    }
}

bool StubEndPoint::SendBinary(const MemoryBuffer& buffer) const
{
    return IsConnected() && !buffer.IsEmpty();
}

bool StubEndPoint::SendText(const std::string& text) const
{
    return IsConnected() && !text.empty();
}

} // namespace RTC
