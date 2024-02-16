#include "RTC/MediaTranslate/TranslatorEndPoint/StubEndPoint.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"

namespace RTC
{

StubEndPoint::StubEndPoint(std::string ownerId)
    : TranslatorEndPoint(std::move(ownerId), "StubEndPoint #" + std::to_string(_instances.fetch_add(1U) + 1U))
{
    _instances.fetch_add(1U);
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
