#pragma once

#include <string>
#include <memory>

namespace RTC
{

class MemoryBuffer;
enum class WebsocketState;
enum class WebsocketFailure;

class WebsocketListener
{
public:
    virtual ~WebsocketListener() = default;
    virtual void OnStateChanged(uint64_t socketId, WebsocketState state);
    virtual void OnFailed(uint64_t socketId, WebsocketFailure failure, const std::string& what);
    virtual void OnTextMessageReceived(uint64_t /*socketId*/, const std::string& /*message*/) {}
    virtual void OnBinaryMessageReceved(uint64_t /*socketId*/, const std::shared_ptr<MemoryBuffer>& /*message*/) {}
};

} // namespace RTC
