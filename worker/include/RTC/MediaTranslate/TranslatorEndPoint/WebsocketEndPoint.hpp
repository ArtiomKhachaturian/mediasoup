#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketListener.hpp"

namespace RTC
{

#define WRITE_TRANSLATION_TO_FILE

class Websocket;

class WebsocketEndPoint : public TranslatorEndPoint, private WebsocketListener
{
public:
    WebsocketEndPoint(std::string ownerId = std::string());
    ~WebsocketEndPoint() final;
    static uint64_t GetInstancesCount() { return _instances.load(); }
    // impl. of TranslatorEndPoint
    bool IsConnected() const final;
protected:
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const MemoryBuffer& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
    static inline const std::string _tsUri = "wss://20.218.159.203:8080/record";
    static inline const std::string _tsUser = "user";
    static inline const std::string _tsUserPassword = "password";
    static inline constexpr uint32_t _defaultTimeSliceMs = 400U;
    static inline std::atomic<uint64_t> _instances = 0ULL;
    const std::string _userAgent;
    const std::unique_ptr<Websocket> _socket;
};

}
