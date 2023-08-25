#include "RTC/OutputWebSocketDevice.hpp"

namespace RTC
{

OutputWebSocketDevice::OutputWebSocketDevice()
{
    struct lws_context_creation_info contextInfo = {};
}

bool OutputWebSocketDevice::Write(const void* buf, uint32_t len)
{
    
}

} // namespace RTC
