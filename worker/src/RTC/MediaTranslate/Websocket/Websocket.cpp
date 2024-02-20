#define MS_CLASS "RTC::Websocket"
#include "RTC/MediaTranslate/Websocket/Websocket.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketListener.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketState.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketFailure.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketFactory.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

namespace {

using namespace RTC;

class FunctorListener : public WebsocketListener
{
public:
    FunctorListener(OnStateChangedFn onStateChanged,
                    OnFailedFn onFailed,
                    OnTextMessageReceivedFn onTextMessageReceived,
                    OnBinaryMessageRecevedFn onBinaryMessageReceved);
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnFailed(uint64_t socketId, WebsocketFailure failure, const std::string& what) final;
    void OnTextMessageReceived(uint64_t socketId, const std::string& message) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
    const OnStateChangedFn _onStateChanged;
    const OnFailedFn _onFailed;
    const OnTextMessageReceivedFn _onTextMessageReceived;
    const OnBinaryMessageRecevedFn _onBinaryMessageReceved;
};

}

namespace RTC
{

Websocket::Websocket()
    : _listeners(std::make_shared<SocketListeners>())
{
}

bool Websocket::WriteBinary(std::vector<uint8_t> buffer)
{
    return WriteBinary(std::make_shared<SimpleMemoryBuffer>(std::move(buffer)));
}

bool Websocket::WriteBinary(const void* buf, size_t len)
{
    return WriteBinary(std::make_shared<SimpleMemoryBuffer>(buf, len));
}

void Websocket::AddListener(WebsocketListener* listener)
{
	_listeners->Add(listener);
}

void Websocket::RemoveListener(WebsocketListener* listener)
{
	_listeners->Remove(listener);
}

void WebsocketOptions::AddAuthorizationHeader(const std::string& user,
                                              const std::string& password)
{
    if (!user.empty() || !password.empty()) {
        auto auth = Utils::String::Base64Encode(user + ":" + password);
        _extraHeaders["Authorization"] = "Basic " + auth;
    }
}

WebsocketOptions WebsocketOptions::CreateWithAuthorization(const std::string& user,
                                                           const std::string& password)
{
    WebsocketOptions options;
    options.AddAuthorizationHeader(user, password);
    return options;
}

WebsocketOptions WebsocketFactory::CreateOptions() const
{
    auto options = WebsocketOptions::CreateWithAuthorization(GetUser(), GetUserPassword());
    options._userAgent = GetAgentName();
    return options;
}

void WebsocketListener::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    MS_DEBUG_DEV_STD("state changed to %s, socked ID %llu", ToString(state), socketId);
}

void WebsocketListener::OnFailed(uint64_t socketId, WebsocketFailure failure, const std::string& what)
{
    MS_ERROR_STD("%s failure - %s, socked ID %llu", ToString(failure), what.c_str(), socketId);
}

WebsocketListener* WebsocketListener::Create(OnStateChangedFn onStateChanged,
                                             OnFailedFn onFailed,
                                             OnTextMessageReceivedFn onTextMessageReceived,
                                             OnBinaryMessageRecevedFn onBinaryMessageReceved)
{
    if (onStateChanged || onFailed || onTextMessageReceived || onBinaryMessageReceved) {
        return new FunctorListener(std::move(onStateChanged), std::move(onFailed),
                                   std::move(onTextMessageReceived),
                                   std::move(onBinaryMessageReceved));
    }
    return nullptr;
}

const char* ToString(WebsocketFailure failure) {
    switch (failure) {
        case WebsocketFailure::General:
            return "general";
        case WebsocketFailure::NoConnection:
            return "no connection";
        case WebsocketFailure::CustomHeader:
            return "custom header";
        case WebsocketFailure::WriteText:
            return "write text";
        case WebsocketFailure::WriteBinary:
            return "write binary";
        case WebsocketFailure::TlsOptions:
            return "TLS options";
        default:
            break;
    }
    return "unknown";
}

const char* ToString(WebsocketState state) {
    switch (state) {
        case WebsocketState::Invalid:
            return "invalid";
        case WebsocketState::Connecting:
            return "connecting";
        case WebsocketState::Connected:
            return "connected";
        case WebsocketState::Disconnected:
            return "disconnected";
        default:
            break;
    }
    return "unknown";
}

} // namespace RTC

namespace {

FunctorListener::FunctorListener(OnStateChangedFn onStateChanged,
                                 OnFailedFn onFailed,
                                 OnTextMessageReceivedFn onTextMessageReceived,
                                 OnBinaryMessageRecevedFn onBinaryMessageReceved)
    : _onStateChanged(std::move(onStateChanged))
    , _onFailed(std::move(onFailed))
    , _onTextMessageReceived(std::move(onTextMessageReceived))
    , _onBinaryMessageReceved(std::move(onBinaryMessageReceved))
{
}

void FunctorListener::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    if (_onStateChanged) {
        _onStateChanged(socketId, state);
    }
}

void FunctorListener::OnFailed(uint64_t socketId, WebsocketFailure failure,
                               const std::string& what)
{
    if (_onFailed) {
        _onFailed(socketId, failure, what);
    }
}

void FunctorListener::OnTextMessageReceived(uint64_t socketId, const std::string& message)
{
    if (_onTextMessageReceived) {
        _onTextMessageReceived(socketId, message);
    }
}

void FunctorListener::OnBinaryMessageReceved(uint64_t socketId,
                                             const std::shared_ptr<MemoryBuffer>& message)
{
    if (_onBinaryMessageReceved) {
        _onBinaryMessageReceved(socketId, message);
    }
}

}
