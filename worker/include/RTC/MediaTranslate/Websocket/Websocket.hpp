#pragma once
#include "RTC/ObjectId.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketState.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketOptions.hpp"
#include "RTC/Listeners.hpp"
#include <memory>
#include <string>

namespace RTC
{

class WebsocketListener;
class Buffer;

class Websocket : public ObjectId
{
protected:
    using SocketListeners = Listeners<WebsocketListener*>;
public:
    virtual ~Websocket() = default;
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual WebsocketState GetState() const = 0;
    virtual std::string GetUrl() const = 0;
    virtual bool WriteText(const std::string& text) = 0;
    virtual bool WriteBinary(const std::shared_ptr<Buffer>& buffer) = 0;
    void AddListener(WebsocketListener* listener);
    void RemoveListener(WebsocketListener* listener);
protected:
    Websocket();
protected:
    // increase read buffer size to optimize for huge audio messages:
    // 64 Kb instead of 16 by default
    static inline constexpr size_t _connectionReadBufferSize = 1 << 16;
    const std::shared_ptr<SocketListeners> _listeners;
};


} // namespace RTC
