#pragma once
#include "RTC/MediaTranslate/Websocket/WebsocketFactory.hpp"

#define LOCAL_WEBSOCKET_TEST_SERVER

namespace RTC
{

class WebsocketTppFactory : public WebsocketFactory
{
public:
    WebsocketTppFactory() = default;
    // impl. of WebsocketFactory
    std::unique_ptr<Websocket> Create() const final;
};

#ifdef LOCAL_WEBSOCKET_TEST_SERVER

class MediaTimer;

class WebsocketTppTestFactory : public WebsocketTppFactory
{
    class MockServer;
public:
    WebsocketTppTestFactory();
    WebsocketTppTestFactory(const std::shared_ptr<MediaTimer>& timer);
    ~WebsocketTppTestFactory() final;
    bool IsValid() const;
    // overrides of WebsocketTppFactory
    std::string GetUri() const final;
private:
    static inline const char*  _filename = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_long.webm";
    static inline constexpr uint32_t _repeatIntervalMs = 15000;
    static inline constexpr uint16_t _port = 8080U;
    static inline const std::string _localUri = "wss://localhost";
    const std::unique_ptr<MockServer> _server;
};
#endif

} // namespace RTC
