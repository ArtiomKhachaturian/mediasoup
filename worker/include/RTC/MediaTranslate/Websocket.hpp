#ifndef MS_RTC_WEBSOCKET_HPP
#define MS_RTC_WEBSOCKET_HPP

#include "MemoryBuffer.h"
#include <string>
#include <memory>
#include <thread>
#include <unordered_map>

namespace RTC
{

class WebsocketListener;

enum class WebsocketState
{
    Invalid, // wrong URI
    Connecting,
    Connected,
    Disconnected,
};

class Websocket
{
    struct UriParts;
    class Socket;
    template<class TConfig> class SocketImpl;
    class SocketTls;
    class SocketNoTls;
public:
    // Supported URI schemes: WS or WSS
    Websocket(const std::string& uri,
              std::unordered_map<std::string, std::string> headers = {},
              std::string tlsTrustStore = std::string(),
              std::string tlsKeyStore = std::string(),
              std::string tlsPrivateKey = std::string(),
              std::string tlsPrivateKeyPassword = std::string());
    ~Websocket();
    bool Open(const std::string& user = std::string(), const std::string& password = std::string());
    void Close();
    WebsocketState GetState() const;
    bool Write(const void* buf, size_t len);
    bool WriteText(const std::string& text);
    void SetListener(const std::shared_ptr<WebsocketListener>& listener);
private:
    const std::shared_ptr<UriParts> _uri;
    const std::shared_ptr<Socket> _socket;
    std::thread _asioThread;
};

class WebsocketListener
{
public:
    virtual ~WebsocketListener() = default;
    virtual void OnStateChanged(WebsocketState /*state*/) {}
    virtual void OnFailed(const std::string& /*what*/) {}
    virtual void OnTextMessageReceived(std::string /*message*/) {}
    virtual void OnBinaryMessageReceved(const std::shared_ptr<MemoryBuffer>& /*message*/) {}
};

} // namespace RTC

#endif
