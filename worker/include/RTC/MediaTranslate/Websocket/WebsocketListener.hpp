#pragma once
#include <functional>
#include <string>
#include <memory>

namespace RTC
{

class Buffer;
enum class WebsocketState;
enum class WebsocketFailure;

class WebsocketListener
{
public:
    using OnStateChangedFn = std::function<void(uint64_t, WebsocketState)>;
    using OnFailedFn = std::function<void(uint64_t, WebsocketFailure, const std::string&)>;
    using OnTextMessageReceivedFn = std::function<void(uint64_t, const std::string&)>;
    using OnBinaryMessageRecevedFn = std::function<void(uint64_t, const std::shared_ptr<Buffer>&)>;
public:
    virtual ~WebsocketListener() = default;
    virtual void OnStateChanged(uint64_t socketId, WebsocketState state);
    virtual void OnFailed(uint64_t socketId, WebsocketFailure failure, const std::string& what);
    virtual void OnTextMessageReceived(uint64_t /*socketId*/, const std::string& /*message*/) {}
    virtual void OnBinaryMessageReceved(uint64_t /*socketId*/, const std::shared_ptr<Buffer>& /*message*/) {}
    static WebsocketListener* Create(OnStateChangedFn onStateChanged = OnStateChangedFn(),
                                     OnFailedFn onFailed = OnFailedFn(),
                                     OnTextMessageReceivedFn onTextMessageReceived = OnTextMessageReceivedFn(),
                                     OnBinaryMessageRecevedFn onBinaryMessageReceved = OnBinaryMessageRecevedFn());
};

} // namespace RTC
