#define MS_CLASS "RTC::Websocket"
#include "RTC/MediaTranslate/Websocket/Websocket.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketListener.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketState.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketFailure.hpp"
#include "Logger.hpp"

namespace RTC
{

Websocket::Websocket()
    : _listeners(std::make_shared<SocketListeners>())
{
}

void Websocket::AddListener(WebsocketListener* listener)
{
	_listeners->Add(listener);
}

void Websocket::RemoveListener(WebsocketListener* listener)
{
	_listeners->Remove(listener);
}

void WebsocketListener::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    MS_DEBUG_DEV_STD("state changed to %s, socked ID %llu", ToString(state), socketId);
}

void WebsocketListener::OnFailed(uint64_t socketId, WebsocketFailure failure, const std::string& what)
{
    MS_ERROR_STD("%s failure - %s, socked ID %llu", ToString(failure), what.c_str(), socketId);
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
