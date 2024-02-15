#define MS_CLASS "RTC::WebsocketEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/Websocket.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketState.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#ifdef WRITE_TRANSLATION_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "DepLibUV.hpp"
#endif
#include "Logger.hpp"

namespace RTC
{

WebsocketEndPoint::WebsocketEndPoint()
    : TranslatorEndPoint(_defaultTimeSliceMs)
    , _socket(std::make_unique<Websocket>(_tsUri, _tsUser, _tsUserPassword))
{
    _socket->AddListener(this);
}

WebsocketEndPoint::~WebsocketEndPoint()
{
    WebsocketEndPoint::Disconnect();
    _socket->RemoveListener(this);
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
                MS_ERROR_STD("failed to connect with translation service %s", GetFullUrl().c_str());
            }
            break;
        default:
            break;
    }
}

void WebsocketEndPoint::Disconnect()
{
    if (IsConnected()) {
        SimpleMemoryBuffer bye;
        _socket->WriteBinary(bye);
    }
    _socket->Close();
}

bool WebsocketEndPoint::SendBinary(const MemoryBuffer& buffer) const
{
    const auto ok = _socket->WriteBinary(buffer);
    if (!ok) {
        MS_ERROR_STD("failed write binary (%zu bytes)' into translation service %s",
                     buffer.GetSize(), GetFullUrl().c_str());
    }
    return ok;
}

bool WebsocketEndPoint::SendText(const std::string& text) const
{
    const auto ok = _socket->WriteText(text);
    if (!ok) {
        MS_ERROR_STD("failed write text '%s' into translation service %s",
                     text.c_str(), GetFullUrl().c_str());
    }
    return ok;
}

std::string WebsocketEndPoint::GetFullUrl() const
{
    return _socket->GetUrl();
}

void WebsocketEndPoint::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    WebsocketListener::OnStateChanged(socketId, state);
    switch (state) {
        case WebsocketState::Connected:
            NotifyThatConnectionEstablished(true);
            MS_ERROR_STD("Connected to %s", GetFullUrl().c_str());
            break;
        case WebsocketState::Disconnected:
            NotifyThatConnectionEstablished(false);
            MS_ERROR_STD("Disconnected from %s", GetFullUrl().c_str());
            break;
        default:
            break;
    }
}

void WebsocketEndPoint::OnBinaryMessageReceved(uint64_t, const std::shared_ptr<MemoryBuffer>& message)
{
    if (message) {
#ifdef WRITE_TRANSLATION_TO_FILE
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = std::string(depacketizerPath) + "/"
                + "received_translation_" + std::to_string(DepLibUV::GetTimeMs()) + ".webm";
            FileWriter::WriteAll(fileName, message);
        }
#endif
        NotifyThatTranslatedMediaReceived(message);
    }
}

}
