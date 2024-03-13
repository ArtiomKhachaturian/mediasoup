#pragma once
#include "UVHandle.hpp"

class UVAsyncHandle
{
public:
    UVAsyncHandle(uv_loop_t* loop, uv_async_cb asyncCb, void* data);
    UVAsyncHandle(UVAsyncHandle&& tmp) = default;
    UVAsyncHandle(const UVAsyncHandle&) = delete;
    UVAsyncHandle& operator = (UVAsyncHandle&& tmp);
    UVAsyncHandle& operator = (const UVAsyncHandle&) = delete;
    int Send() const;
    void Invoke() const;
    operator uv_async_t* () const { return _handle.GetHandle(); }
private:
    UVHandle<uv_async_t> _handle;
};
