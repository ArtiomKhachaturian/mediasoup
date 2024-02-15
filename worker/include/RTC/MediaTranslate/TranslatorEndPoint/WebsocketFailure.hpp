#pragma once


namespace RTC
{

enum class WebsocketFailure
{
    General,
    NoConnection,
    CustomHeader,
    WriteText,
    WriteBinary,
    TlsOptions
};

const char* ToString(WebsocketFailure failure);

} // namespace RTC