#pragma once
#include "RTC/MediaTranslate/Websocket/WebsocketFactory.hpp"

namespace RTC
{

class BufferAllocator;

class WebsocketTppFactory : public WebsocketFactory
{
public:
    static std::unique_ptr<WebsocketFactory> CreateFactory(const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    // impl. of WebsocketFactory
    std::unique_ptr<Websocket> Create() const final;
protected:
    WebsocketTppFactory() = default;
};

} // namespace RTC
