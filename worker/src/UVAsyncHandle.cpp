#define MS_CLASS "RTC::UVAsyncHandle"
#include "UVAsyncHandle.hpp"
#include "Logger.hpp"

namespace RTC
{

UVAsyncHandle::UVAsyncHandle(uv_loop_t* loop, uv_async_cb asyncCb, void* data)
    : _handle(UVHandle<uv_async_t>::CreateInitialized(loop, asyncCb))
{
    MS_ASSERT(_handle, "uv_async_init() failed");
    _handle->data = data;
}

void UVAsyncHandle::Invoke() const
{
    const auto ret = uv_async_send(_handle.GetHandle());
    if (0 != ret) {
        MS_ERROR_STD("uv_async_send() failed: %s", uv_strerror(ret));
    }
}

} // namespace RTC
