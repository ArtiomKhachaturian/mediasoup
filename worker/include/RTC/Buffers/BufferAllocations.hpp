#pragma once

#include "RTC/InheritanceSelector.hpp"
#include "RTC/Buffers/BufferAllocator.hpp"

namespace RTC
{

template<class TBase>
class BufferAllocations : public InheritanceSelector<TBase>
{
public:
    const std::weak_ptr<BufferAllocator>& GetAllocator() const { return _allocator; }
    std::shared_ptr<Buffer> AllocateBuffer(size_t size) const;
    std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data) const;
    std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data, size_t dataSize) const;
    std::shared_ptr<Buffer> ReallocateBuffer(size_t size, const std::shared_ptr<Buffer>& buffer) const;
protected:
    template <class... Args>
    BufferAllocations(const std::weak_ptr<BufferAllocator>& allocator, Args&&... args);
private:
    const std::weak_ptr<BufferAllocator> _allocator;
};

template<class TBase>
template <class... Args>
inline BufferAllocations<TBase>::BufferAllocations(const std::weak_ptr<BufferAllocator>& allocator,
                                                   Args&&... args)
    : InheritanceSelector<TBase>(std::forward<Args>(args)...)
    , _allocator(allocator)
{
}

template<class TBase>
inline std::shared_ptr<Buffer> BufferAllocations<TBase>::AllocateBuffer(size_t size) const
{
    return RTC::AllocateBuffer(size, GetAllocator());
}

template<class TBase>
inline std::shared_ptr<Buffer> BufferAllocations<TBase>::AllocateBuffer(size_t size,
                                                                        const void* data) const
{
    return RTC::AllocateBuffer(size, data, GetAllocator());
}

template<class TBase>
inline std::shared_ptr<Buffer> BufferAllocations<TBase>::AllocateBuffer(size_t size,
                                                                        const void* data,
                                                                        size_t dataSize) const
{
    return RTC::AllocateBuffer(size, data, dataSize, GetAllocator());
}

template<class TBase>
std::shared_ptr<Buffer> BufferAllocations<TBase>::ReallocateBuffer(size_t size,
                                                                   const std::shared_ptr<Buffer>& buffer) const
{
    return RTC::ReallocateBuffer(size, buffer, GetAllocator());
}

} // namespace RTC
