#pragma once

#include "RTC/MediaTranslate/WebsocketState.hpp"
#include <string>
#include <memory>
#include <thread>
#include <unordered_map>
#include <mutex>

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
    using Mutex = std::recursive_mutex;
    using ReadLock = std::lock_guard<Mutex>;
    using WriteLock = ReadLock;
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
    std::shared_ptr<Socket> _socket;
    std::thread _socketAsioThread;
    mutable Mutex _socketMutex;
};

} // namespace RTC
