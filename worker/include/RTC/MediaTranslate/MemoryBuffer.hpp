#pragma once
#include "RTC/MediaTranslate/MediaObject.hpp"
#include <memory>
#include <type_traits>

namespace RTC
{

class MemoryBuffer : public MediaObject
{
public:
    virtual size_t GetSize() const = 0;
    virtual uint8_t* GetData() = 0;
    virtual const uint8_t* GetData() const = 0;
    virtual void Recycle() {}
    bool IsEmpty() const { return 0U == GetSize(); }
};

template<class TBufferImpl, class... Args>
inline std::shared_ptr<TBufferImpl> MakeMemoryBuffer(Args&&... args) {
    static_assert(std::is_base_of_v<MemoryBuffer, TBufferImpl>, "class must be derived from MemoryBuffer");
    auto buffer = new TBufferImpl(std::forward<Args>(args)...);
    return std::shared_ptr<TBufferImpl>(buffer, [](TBufferImpl* buffer) {
        buffer->Recycle();
        delete buffer;
    });
}

} // namespace RTC
