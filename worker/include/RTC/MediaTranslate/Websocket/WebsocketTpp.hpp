#pragma once
#include "RTC/MediaTranslate/Websocket/Websocket.hpp"
#include "ProtectedObj.hpp"

namespace RTC
{

class WebsocketListener;
class MemoryBuffer;
enum class WebsocketState;

class WebsocketTpp : public Websocket
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
    WebsocketTpp(const std::string& uri, WebsocketOptions options = WebsocketOptions());
    ~WebsocketTpp() final;
    bool Open() final;
    void Close() final;
    WebsocketState GetState() const final;
    std::string GetUrl() const final;
    bool WriteBinary(const std::shared_ptr<MemoryBuffer>& buffer) final;
    bool WriteText(const std::string& text) final;
private:
    const std::shared_ptr<const Config> _config;
    ProtectedUniquePtr<Socket> _socket;
};

} // namespace RTC
