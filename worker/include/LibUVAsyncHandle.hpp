#pragma once
#include "LibUVHandle.hpp"

namespace RTC
{

class LibUVAsyncHandle : public LibUVHandle<uv_async_t>
{
public:
    LibUVAsyncHandle(uv_async_cb asyncCb, void* data) noexcept(false);
    LibUVAsyncHandle(uv_loop_t* loop, uv_async_cb asyncCb, void* data) noexcept(false);
    void Invoke() const noexcept(false);
private:
    static uv_async_t* AllocHandle(uv_loop_t* loop, uv_async_cb asyncCb, void* data) noexcept(false);
};

} // namespace RTC
