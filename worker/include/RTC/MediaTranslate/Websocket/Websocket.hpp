#pragma once

#include "ProtectedObj.hpp"
#include "RTC/MediaTranslate/MediaObject.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketOptions.hpp"
#include "RTC/Listeners.hpp"
#include <string>
#include <memory>

namespace RTC
{

class WebsocketListener;
class MemoryBuffer;
enum class WebsocketState;

class Websocket : public MediaObject
{
    class Config;
    class Socket;
    template<class TConfig> class SocketImpl;
    template <class TConfig> struct SocketConfig;
    class SocketTls;
    class SocketNoTls;
    class SocketWrapper;
    using SocketListeners = Listeners<WebsocketListener*>;
public:
    Websocket(const std::string& uri,
              const std::string& user = std::string(),
              const std::string& password = std::string(),
              WebsocketOptions options = WebsocketOptions());
    ~Websocket() final;
    bool Open();
    void Close();
    WebsocketState GetState() const;
    std::string GetUrl() const;
    bool WriteBinary(const MemoryBuffer& buffer);
    bool WriteText(const std::string& text);
    void AddListener(WebsocketListener* listener);
    void RemoveListener(WebsocketListener* listener);
private:
    // increase read buffer size to optimize for huge audio messages:
    // 64 Kb instead of 16 by default
    static inline constexpr size_t _connectionReadBufferSize = 1024U * 64U;
    const std::shared_ptr<const Config> _config;
    const std::shared_ptr<SocketListeners> _listeners;
    ProtectedUniquePtr<Socket> _socket;
};

} // namespace RTC
