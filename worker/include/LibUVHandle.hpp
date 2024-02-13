#pragma once
#include <uv.h>

namespace RTC
{

template<typename THandle>
class LibUVHandle
{
public:
	// takes ownership
    LibUVHandle(THandle* handle) { Reset(handle); }
	~LibUVHandle() { Reset(nullptr); }
    THandle* GetHandle() const { return _handle; }
    operator THandle* () const { return GetHandle(); }
    bool IsValid() const { return nullptr != GetHandle(); }
	void Reset(THandle* handle = nullptr);
private:
	static void OnClosed(uv_handle_t* handle);
private:
	THandle* _handle = nullptr;
};

template<typename THandle>
void LibUVHandle<THandle>::Reset(THandle* handle)
{
	if (handle != _handle) {
		if (_handle) {
			uv_close(reinterpret_cast<uv_handle_t*>(_handle), OnClosed);
		}
		_handle = handle;
	}
}

template<typename THandle>
void LibUVHandle<THandle>::OnClosed(uv_handle_t* handle)
{
	delete reinterpret_cast<THandle*>(handle);
}

}
