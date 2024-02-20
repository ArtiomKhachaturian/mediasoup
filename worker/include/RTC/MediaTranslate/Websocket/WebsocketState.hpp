#pragma once

namespace RTC
{

enum class WebsocketState
{
    Invalid, // wrong URI
    Connecting,
    Connected,
    Disconnecting,
    Disconnected,
};

const char* ToString(WebsocketState state);

} // namespace RTC
