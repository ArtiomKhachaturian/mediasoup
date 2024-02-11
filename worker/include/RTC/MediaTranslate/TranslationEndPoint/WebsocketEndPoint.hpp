#pragma once
#include "RTC/MediaTranslate/TranslationEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslationEndPoint/WebsocketListener.hpp"

namespace RTC
{

#define WRITE_TRANSLATION_TO_FILE

class Websocket;

class WebsocketEndPoint : public TranslatorEndPoint, private WebsocketListener
{
public:
    WebsocketEndPoint(uint64_t id, const std::string& serviceUri,
                      const std::string& serviceUser = std::string(),
                      const std::string& servicePassword = std::string(),
                      const std::string& userAgent = std::string(),
                      uint32_t timeSliceMs = 400U);
    ~WebsocketEndPoint();
protected:
#ifdef WRITE_TRANSLATION_TO_FILE
    void StartMediaWriting(uint32_t ssrc) final;
    void EndMediaWriting(uint32_t ssrc) final;
#endif
    // impl. of TranslatorEndPoint
    bool IsConnected() const final;
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const MemoryBuffer& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
    const std::string _userAgent;
    const std::unique_ptr<Websocket> _socket;
#ifdef WRITE_TRANSLATION_TO_FILE
    std::atomic<uint32_t> _ssrc = 0U;
#endif
};

}
