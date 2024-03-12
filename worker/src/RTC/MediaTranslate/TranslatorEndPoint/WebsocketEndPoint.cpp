#define MS_CLASS "RTC::WebsocketEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket/Websocket.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketFactory.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include "RTC/Buffers/SimpleBuffer.hpp"
#ifdef WRITE_TRANSLATION_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "DepLibUV.hpp"
#endif
#include "Logger.hpp"

namespace {

inline std::string GetUrl(const std::unique_ptr<RTC::Websocket>& socket) {
    return socket ? socket->GetUrl() : std::string();
}

}

namespace RTC
{

WebsocketEndPoint::WebsocketEndPoint(std::unique_ptr<Websocket> socket, std::string ownerId)
    : TranslatorEndPoint(std::move(ownerId), GetUrl(socket))
    , _socket(std::move(socket))
{
    MS_ASSERT(_socket, "websocket must not be null");
    _instances.fetch_add(1U);
    _socket->AddListener(this);
}

std::shared_ptr<WebsocketEndPoint> WebsocketEndPoint::Create(const WebsocketFactory* factory,
                                                             std::string ownerId)
{
    std::shared_ptr<WebsocketEndPoint> endPoint;
    if (factory) {
        if (auto socket = factory->Create()) {
            endPoint = std::make_shared<WebsocketEndPoint>(std::move(socket), std::move(ownerId));
        }
        else {
            MS_ERROR_STD("failed to create websocket");
        }
    }
    return endPoint;
}

WebsocketEndPoint::~WebsocketEndPoint()
{
    WebsocketEndPoint::Disconnect();
    _socket->RemoveListener(this);
    _instances.fetch_sub(1U);
}

bool WebsocketEndPoint::IsConnected() const
{
    return WebsocketState::Connected == _socket->GetState();
}

void WebsocketEndPoint::Connect()
{
    switch (_socket->GetState()) {
        case WebsocketState::Disconnected:
            if (!_socket->Open()) {
                MS_ERROR_STD("failed to connect with translation service %s", GetDescription().c_str());
            }
            break;
        default:
            break;
    }
}

void WebsocketEndPoint::Disconnect()
{
    if (IsConnected()) {
        _socket->WriteBinary(std::make_shared<SimpleBuffer>());
    }
    _socket->Close();
}

bool WebsocketEndPoint::SendBinary(const std::shared_ptr<Buffer>& buffer) const
{
    return _socket->WriteBinary(buffer);
}

bool WebsocketEndPoint::SendText(const std::string& text) const
{
    return _socket->WriteText(text);
}

void WebsocketEndPoint::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    WebsocketListener::OnStateChanged(socketId, state);
    switch (state) {
        case WebsocketState::Connected:
            NotifyThatConnectionEstablished(true);
            break;
        case WebsocketState::Disconnected:
            NotifyThatConnectionEstablished(false);
            break;
        default:
            break;
    }
}

void WebsocketEndPoint::OnBinaryMessageReceved(uint64_t, const std::shared_ptr<Buffer>& message)
{
    if (message) {
#ifdef WRITE_TRANSLATION_TO_FILE
        const auto num = NotifyThatTranslationReceived(message);
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = std::string(depacketizerPath) + "/"
                + "received_translation_#" + std::to_string(num)
                + "_" + std::to_string(DepLibUV::GetTimeMs()) + ".webm";
            FileWriter::WriteAll(fileName, message);
        }
#else
        NotifyThatTranslationReceived(message);
#endif
    }
}

}
