#include "RTC/MediaTranslate/TranslatorEndPoint/StubEndPoint.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"

namespace RTC
{

StubEndPoint::StubEndPoint()
    : _index(_instances.fetch_add(1U))
{
    SetName("StubEndPoint #" + std::to_string(_index + 1U));
}

StubEndPoint::~StubEndPoint()
{
    StubEndPoint::Disconnect();
    _instances.fetch_sub(1U);
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
