#pragma once
#include "RTC/MediaTranslate/Websocket/Websocket.hpp"
#include "ProtectedObj.hpp"
#include <asio/ssl/context.hpp>

namespace RTC
{

class WebsocketListener;
struct WebsocketTls;
enum class WebsocketState;

class WebsocketTpp : public Websocket
{
    class Config;
    class Api;
    template<class TConfig> class Impl;
    class TlsOn;
    class TlsOff;
    class Wrapper;
public:
    WebsocketTpp(const std::string& uri, WebsocketOptions options = WebsocketOptions());
    ~WebsocketTpp() final;
    // impl. of Websocket
    bool Open() final;
    void Close() final;
    WebsocketState GetState() const final;
    std::string GetUrl() const final;
    bool WriteText(const std::string& text) final;
    bool WriteBinary(const std::shared_ptr<Buffer>& buffer) final;
private:
    const std::shared_ptr<const Config> _config;
    ProtectedUniquePtr<Api> _api;
};

} // namespace RTC
