#define MS_CLASS "RTC::LibUVAsyncHandle"
#include "LibUVAsyncHandle.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include <memory>

namespace RTC
{

LibUVAsyncHandle::LibUVAsyncHandle(uv_async_cb asyncCb, void* data) noexcept(false)
    : LibUVAsyncHandle(DepLibUV::GetLoop(), asyncCb, data)
{
}

LibUVAsyncHandle::LibUVAsyncHandle(uv_loop_t* loop, uv_async_cb asyncCb,
                                   void* data) noexcept(false)
    : LibUVHandle<uv_async_t>(AllocHandle(loop, asyncCb, data))
{
}

void LibUVAsyncHandle::Invoke() const noexcept(false)
{
    if (const auto handle = GetHandle()) {
        const auto ret = uv_async_send(handle);
        if (0 != ret) {
            MS_THROW_ERROR_STD("uv_async_send() failed: %s", uv_strerror(ret));
        }
    }
}

uv_async_t* LibUVAsyncHandle::AllocHandle(uv_loop_t* loop, uv_async_cb asyncCb,
                                          void* data) noexcept(false)
{
    std::unique_ptr<uv_async_t> handle;
    if (loop) {
        handle = std::make_unique<uv_async_t>();
        const auto ret = uv_async_init(loop, handle.get(), asyncCb);
        if (0 == ret) {
            handle->data = data;
        }
        else {
            handle.reset();
            MS_THROW_ERROR_STD("uv_async_init() failed: %s", uv_strerror(ret));
        }
    }
    else {
        MS_THROW_ERROR_STD("uv_async_t creation failed: input UV loop is null");
    }
    return handle.release();
}

} // namespace RTC
