#define MS_CLASS "RTC::UVAsyncHandle"
#include "UVAsyncHandle.hpp"
#include "Logger.hpp"

UVAsyncHandle::UVAsyncHandle(uv_loop_t* loop, uv_async_cb asyncCb, void* data)
    : _handle(UVHandle<uv_async_t>::CreateInitialized(loop, asyncCb))
{
    _handle->data = data;
}

int UVAsyncHandle::Send() const
{
    return uv_async_send(_handle.GetHandle());
}

void UVAsyncHandle::Invoke() const
{
    const auto ret = Send();
    if (0 != ret) {
        MS_ERROR_STD("uv_async_send() failed: %s", uv_strerror(ret));
    }
}
