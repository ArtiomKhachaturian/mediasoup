#pragma once

namespace RTC
{

enum class WebsocketState
{
    Invalid, // wrong URI
    Connecting,
    Connected,
    Disconnected,
};

const char* ToString(WebsocketState state);

} // namespace RTC
