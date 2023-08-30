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

}