#pragma once

#include "ProtectedObj.hpp"
#include "RTC/MediaTranslate/WebsocketState.hpp"
#include "RTC/Listeners.hpp"
#include <string>
#include <memory>
#include <unordered_map>

namespace RTC
{

class WebsocketListener;
class MemoryBuffer;

class Websocket
{
    class Config;
    class Socket;
    template<class TConfig> class SocketImpl;
    class SocketTls;
    class SocketNoTls;
    class SocketWrapper;
    using SocketListeners = Listeners<WebsocketListener*>;
public:
    Websocket(const std::string& uri,
              const std::string& user = std::string(),
              const std::string& password = std::string(),
              std::unordered_map<std::string, std::string> headers = {},
              std::string tlsTrustStore = std::string(),
              std::string tlsKeyStore = std::string(),
              std::string tlsPrivateKey = std::string(),
              std::string tlsPrivateKeyPassword = std::string());
    ~Websocket();
    bool Open(const std::string& userAgent = std::string());
    void Close();
    WebsocketState GetState() const;
    uint64_t GetId() const;
    bool WriteBinary(const MemoryBuffer& buffer);
    bool WriteText(const std::string& text);
    void AddListener(WebsocketListener* listener);
    void RemoveListener(WebsocketListener* listener);
private:
    const std::shared_ptr<const Config> _config;
    const std::shared_ptr<SocketListeners> _listeners;
    ProtectedUniquePtr<Socket> _socket;
};

} // namespace RTC
