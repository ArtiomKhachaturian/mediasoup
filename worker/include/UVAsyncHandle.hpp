#pragma once
#include "UVHandle.hpp"

namespace RTC
{

class UVAsyncHandle
{
public:
    UVAsyncHandle(uv_loop_t* loop, uv_async_cb asyncCb, void* data);
    UVAsyncHandle(UVAsyncHandle&& tmp) = default;
    UVAsyncHandle(const UVAsyncHandle&) = delete;
    UVAsyncHandle& operator = (UVAsyncHandle&& tmp);
    UVAsyncHandle& operator = (const UVAsyncHandle&) = delete;
    void Invoke() const;
private:
    UVHandle<uv_async_t> _handle;
};

} // namespace RTC
