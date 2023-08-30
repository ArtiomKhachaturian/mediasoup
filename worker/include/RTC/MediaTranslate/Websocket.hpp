#pragma once

#include "ProtectedObj.hpp"
#include "RTC/MediaTranslate/WebsocketState.hpp"
#include <string>
#include <memory>
#include <unordered_map>

namespace RTC
{

class WebsocketListener;

class Websocket
{
    class Config;
    class Socket;
    template<class TConfig> class SocketImpl;
    class SocketTls;
    class SocketNoTls;
    class SocketWrapper;
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
    bool Write(const void* buf, size_t len);
    bool WriteText(const std::string& text);
    void SetListener(const std::shared_ptr<WebsocketListener>& listener);
private:
    const std::shared_ptr<const Config> _config;
    std::shared_ptr<WebsocketListener> _listener;
    ProtectedUniquePtr<Socket> _socket;
};

} // namespace RTC
