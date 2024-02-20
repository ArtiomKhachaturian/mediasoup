#pragma once
#include "RTC/MediaTranslate/MediaObject.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketState.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketOptions.hpp"
#include "RTC/Listeners.hpp"
#include <memory>
#include <string>

namespace RTC
{

class WebsocketListener;
class MemoryBuffer;

class Websocket : public MediaObject
{
protected:
    using SocketListeners = Listeners<WebsocketListener*>;
public:
    void AddListener(WebsocketListener* listener);
    void RemoveListener(WebsocketListener* listener);
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual WebsocketState GetState() const = 0;
    virtual std::string GetUrl() const = 0;
    virtual bool WriteBinary(const std::shared_ptr<MemoryBuffer>& buffer) = 0;
    virtual bool WriteText(const std::string& text) = 0;
protected:
    Websocket();
protected:
    // increase read buffer size to optimize for huge audio messages:
    // 64 Kb instead of 16 by default
    static inline constexpr size_t _connectionReadBufferSize = 1 << 16;
    const std::shared_ptr<SocketListeners> _listeners;
};


} // namespace RTC
