#define MS_CLASS "RTC::WebsocketEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket/Websocket.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketFactory.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#ifdef WRITE_TRANSLATION_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "DepLibUV.hpp"
#endif
#include "Logger.hpp"

namespace RTC
{

WebsocketEndPoint::WebsocketEndPoint(const WebsocketFactory& factory, std::string ownerId)
    : TranslatorEndPoint(std::move(ownerId), factory.GetUri(), _defaultTimeSliceMs)
    , _socket(factory.Create())
{
    MS_ASSERT(_socket, "failed to create websocket");
    _instances.fetch_add(1U);
    _socket->AddListener(this);
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
        _socket->WriteBinary(std::make_shared<SimpleMemoryBuffer>());
    }
    _socket->Close();
}

bool WebsocketEndPoint::SendBinary(const std::shared_ptr<MemoryBuffer>& buffer) const
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

void WebsocketEndPoint::OnBinaryMessageReceved(uint64_t, const std::shared_ptr<MemoryBuffer>& message)
{
    if (message) {
        MS_ERROR_STD("Received translation #%llu from %s", ++_receivedMessagesCount, GetDescription().c_str());
#ifdef WRITE_TRANSLATION_TO_FILE
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = std::string(depacketizerPath) + "/"
                + "received_translation_#" + std::to_string(_receivedMessagesCount)
                + "_" + std::to_string(DepLibUV::GetTimeMs()) + ".webm";
            FileWriter::WriteAll(fileName, message);
        }
#endif
        Commit(message);
    }
}

}
