#define MS_CLASS "RTC::WebsocketEndPoint"
#include "RTC/MediaTranslate/TranslationEndPoint/WebsocketEndPoint.hpp"
#include "RTC/MediaTranslate/TranslationEndPoint/Websocket.hpp"
#include "RTC/MediaTranslate/TranslationEndPoint/WebsocketState.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#ifdef WRITE_TRANSLATION_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "DepLibUV.hpp"
#endif
#include "Logger.hpp"

namespace RTC
{

WebsocketEndPoint::WebsocketEndPoint(uint64_t id, const std::string& serviceUri,
                                     const std::string& serviceUser,
                                     const std::string& servicePassword,
                                     const std::string& userAgent,
                                     uint32_t timeSliceMs)
    : TranslatorEndPoint(id, timeSliceMs)
    , _userAgent(userAgent)
    , _socket(std::make_unique<Websocket>(serviceUri, serviceUser, servicePassword))
{
    _socket->AddListener(this);
}

WebsocketEndPoint::~WebsocketEndPoint()
{
    WebsocketEndPoint::Disconnect();
    _socket->RemoveListener(this);
}

#ifdef WRITE_TRANSLATION_TO_FILE
void WebsocketEndPoint::StartMediaWriting(uint32_t ssrc)
{
    TranslatorEndPoint::StartMediaWriting(ssrc);
    _ssrc = ssrc;
}

void WebsocketEndPoint::EndMediaWriting(uint32_t ssrc)
{
    TranslatorEndPoint::EndMediaWriting(ssrc);
    _ssrc.compare_exchange_strong(ssrc, 0U);
}
#endif

bool WebsocketEndPoint::IsConnected() const
{
    return WebsocketState::Connected == _socket->GetState();
}

void WebsocketEndPoint::Connect()
{
    switch (_socket->GetState()) {
        case WebsocketState::Disconnected:
            if (!_socket->Open(_userAgent)) {
                MS_ERROR_STD("failed to connect with translation service %s",
                             _socket->GetUrl().c_str());
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
                     buffer.GetSize(), _socket->GetUrl().c_str());
    }
    return ok;
}

bool WebsocketEndPoint::SendText(const std::string& text) const
{
    const auto ok = _socket->WriteText(text);
    if (!ok) {
        MS_ERROR_STD("failed write text '%s' into translation service %s",
                     text.c_str(), _socket->GetUrl().c_str());
    }
    return ok;
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
#ifdef WRITE_TRANSLATION_TO_FILE
        if (const auto ssrc = _ssrc.load()) {
            const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
            if (depacketizerPath && std::strlen(depacketizerPath)) {
                std::string fileName = std::string(depacketizerPath) + "/"
                    + "received_translation_" + std::to_string(ssrc) + "_"
                    + std::to_string(DepLibUV::GetTimeMs()) + ".webm";
                FileWriter::WriteAll(fileName, message);
            }
        }
#endif
        NotifyThatTranslatedMediaReceived(message);
    }
}

}
