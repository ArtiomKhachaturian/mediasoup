#pragma once
#include <uv.h>
#include <cstdio>  // std::snprintf(), std::fprintf(), stdout, stderrv
#include <memory>

namespace RTC
{

template<typename THandle, class... InitArgs>
inline int InitHandle(THandle* handle, InitArgs&&... /*args*/) {
    return -1;
}

inline int InitHandle(uv_loop_t* handle) {
    if (handle) {
        return uv_loop_init(handle);
    }
    return -1;
}

inline int InitHandle(uv_timer_t* handle, uv_loop_t* loop) {
    if (handle && loop) {
        return uv_timer_init(loop, handle);
    }
    return -1;
}

inline int InitHandle(uv_async_t* handle, uv_loop_t* loop, uv_async_cb asyncCb) {
    if (handle && loop) {
        return uv_async_init(loop, handle, asyncCb);
    }
    return -1;
}

template<typename THandle>
inline void CloseUVHandle(THandle* handle) {
    if (handle) {
        uv_close(reinterpret_cast<uv_handle_t*>(handle), [](uv_handle_t* handle) {
            delete reinterpret_cast<THandle*>(handle);
        });
    }
}

template<>
inline void CloseUVHandle<uv_loop_t>(uv_loop_t* loop) {
    if (loop) {
        while (true) {
            if (UV_EBUSY != uv_loop_close(loop)) {
                break;
            }
            uv_run(loop, UV_RUN_NOWAIT);
        }
        delete loop;
    }
}

template<typename THandle>
class UVHandle final
{
public:
    // takes ownership
    explicit UVHandle(THandle* handle);
    UVHandle(UVHandle&& tmp);
    UVHandle(const UVHandle&) = delete;
    ~UVHandle() { Reset(nullptr); }
    UVHandle& operator = (UVHandle&& tmp);
    UVHandle& operator = (const UVHandle&) = delete;
    THandle* GetHandle() const { return _handle; }
    THandle* operator -> () const { return GetHandle(); }
    explicit operator bool () const { return IsValid(); }
    bool IsValid() const { return nullptr != GetHandle(); }
    void Reset(THandle* handle = nullptr);
    template<class... InitArgs>
    static UVHandle CreateInitialized(InitArgs&&... args);
private:
    THandle* _handle = nullptr;
};

template<typename THandle>
UVHandle<THandle>::UVHandle(THandle* handle)
    : _handle(handle)
{
}

template<typename THandle>
UVHandle<THandle>::UVHandle(UVHandle&& tmp)
    : _handle(nullptr)
{
    std::swap(_handle, tmp._handle);
}

template<typename THandle>
UVHandle<THandle>& UVHandle<THandle>::operator = (UVHandle&& tmp)
{
    if (&tmp != this) {
        std::swap(_handle, tmp._handle);
    }
    return *this;
}

template<typename THandle>
void UVHandle<THandle>::Reset(THandle* handle)
{
    if (handle != _handle) {
        if (_handle && 0 == uv_is_closing(reinterpret_cast<uv_handle_t*>(_handle))) {
            CloseUVHandle(_handle);
        }
        _handle = handle;
    }
}

template<typename THandle> template<class... InitArgs>
UVHandle<THandle> UVHandle<THandle>::CreateInitialized(InitArgs&&... args)
{
    auto handle = std::make_unique<THandle>();
    const auto ret = InitHandle(handle.get(), std::forward<InitArgs>(args)...);
    if (0 != ret) {
        std::fprintf(stderr, "initialization of UV handle failed: %s", uv_strerror(ret));
        std::fflush(stderr);
        handle.reset();
    }
    return UVHandle(handle.release());
}

} // namespace RTC
