#pragma once

#include "MemoryBuffer.h"
#include "RTC/MediaTranslate/WebsocketState.hpp"
#include <string>
#include <memory>

namespace RTC
{

class WebsocketListener
{
public:
    enum class FailureType
    {
        General,
        NoConnection,
        WriteText,
        WriteBinary,
        TlsOptions
    };
public:
    virtual ~WebsocketListener() = default;
    virtual void OnStateChanged(uint64_t socketId, WebsocketState state);
    virtual void OnFailed(uint64_t socketId, FailureType type, std::string what);
    virtual void OnTextMessageReceived(uint64_t /*socketId*/, std::string /*message*/) {}
    virtual void OnBinaryMessageReceved(uint64_t /*socketId*/, const std::shared_ptr<MemoryBuffer>& /*message*/) {}
};

} // namespace RTC
