#ifndef MS_RTC_OUTPUT_WEBSOCKET_DEVICE_HPP
#define MS_RTC_OUTPUT_WEBSOCKET_DEVICE_HPP

#include "RTC/OutputDevice.hpp"
#include <libwebsockets.h>
#include <thread>

namespace RTC
{

class OutputWebSocketDevice : public OutputDevice
{
public:
    OutputWebSocketDevice();
    // impl. of OutputDevice
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _position; }
    bool SetPosition(int64_t /*position*/) final { return false; }
    bool Seekable() const final { return false; }
    bool IsFileDevice() const final { return false; }
private:
    struct lws_context* _context = nullptr;
    struct lws* _onnection = nullptr;
    int64_t _position = 0LL;
};

} // namespace RTC

#endif
