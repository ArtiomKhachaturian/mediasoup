#include "RTC/MediaTranslate/TranslationEndPoint/MockEndPoint.hpp"

namespace RTC
{

MockEndPoint::MockEndPoint(uint64_t id)
    : TranslatorEndPoint(id, 0U)
{
}

MockEndPoint::~MockEndPoint()
{
    MockEndPoint::Disconnect();
}

bool MockEndPoint::IsConnected() const
{
    return _connected.load();
}

void MockEndPoint::Connect()
{
    if (!_connected.exchange(true)) {
        NotifyThatConnectionEstablished(true);
    }
}

void MockEndPoint::Disconnect()
{
    if (_connected.exchange(false)) {
        NotifyThatConnectionEstablished(false);
    }
}

bool MockEndPoint::SendBinary(const MemoryBuffer&) const
{
    return IsConnected();
}

bool MockEndPoint::SendText(const std::string&) const
{
    return IsConnected();
}

}
