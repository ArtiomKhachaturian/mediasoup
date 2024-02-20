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
    WebsocketTppTestFactory(std::string fileName, uint32_t repeatIntervalMs);
    WebsocketTppTestFactory(const std::shared_ptr<MediaTimer>& timer,
                            std::string fileName, uint32_t repeatIntervalMs);
    ~WebsocketTppTestFactory() final;
    bool IsValid() const;
    // overrides of WebsocketTppFactory
    std::string GetUri() const final;
private:
    static inline const uint16_t _port = 8080;
    static inline const std::string _localUri = "wss://localhost";
    const std::unique_ptr<MockServer> _server;
};
#endif

} // namespace RTC
