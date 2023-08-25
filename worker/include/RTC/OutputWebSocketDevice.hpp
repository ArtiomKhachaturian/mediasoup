#ifndef MS_RTC_OUTPUT_WEBSOCKET_DEVICE_HPP
#define MS_RTC_OUTPUT_WEBSOCKET_DEVICE_HPP

#include "RTC/OutputDevice.hpp"
#include <libwebsockets.h>
#include <thread>
#include <string>
#include <memory>
#include <unordered_map>

namespace RTC
{

class OutputWebSocketDevice : public OutputDevice
{
    struct UriParts;
public:
    OutputWebSocketDevice(const std::string& uri,
                          std::unordered_map<std::string, std::string> extraHeaders = {});
    OutputWebSocketDevice(const std::string& baseUri, bool secure,
                          std::unordered_map<std::string, std::string> extraHeaders = {});
    ~OutputWebSocketDevice() final;
    // impl. of OutputDevice
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _position; }
    bool SetPosition(int64_t /*position*/) final { return false; }
    bool Seekable() const final { return false; }
    bool IsFileDevice() const final { return false; }
private:
    const std::unique_ptr<UriParts> _uriParts;
    const std::unordered_map<std::string, std::string> _extraHeaders;
    std::unique_ptr<struct lws_protocols[]> _protocols;
    struct lws_context* _context = nullptr;
    struct lws* _onnection = nullptr;
    int64_t _position = 0LL;
};

} // namespace RTC

#endif
