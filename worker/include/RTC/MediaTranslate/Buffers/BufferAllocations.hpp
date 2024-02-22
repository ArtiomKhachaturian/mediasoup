#pragma once
#include "RTC/MediaTranslate/Buffers/BufferAllocator.hpp"

namespace RTC
{

template<class TBase>
class BufferAllocations : public TBase
{
public:
    const std::weak_ptr<BufferAllocator>& GetAllocator() const { return _allocator; }
    std::shared_ptr<Buffer> AllocateBuffer(size_t size) const;
    std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data) const;
    std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data, size_t dataSize) const;
protected:
    template <class... Args>
    BufferAllocations(const std::weak_ptr<BufferAllocator>& allocator, Args&&... args);
private:
    const std::weak_ptr<BufferAllocator> _allocator;
};

template<>
class BufferAllocations<void>
{
public:
    const std::weak_ptr<BufferAllocator>& GetAllocator() const { return _allocator; }
    std::shared_ptr<Buffer> AllocateBuffer(size_t size) const;
    std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data) const;
    std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data, size_t dataSize) const;
protected:
    BufferAllocations(const std::weak_ptr<BufferAllocator>& allocator);
private:
    const std::weak_ptr<BufferAllocator> _allocator;
};

template<class TBase>
template <class... Args>
inline BufferAllocations<TBase>::BufferAllocations(const std::weak_ptr<BufferAllocator>& allocator,
                                                   Args&&... args)
    : TBase(std::forward<Args>(args)...)
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

inline BufferAllocations<void>::BufferAllocations(const std::weak_ptr<BufferAllocator>& allocator)
    : _allocator(allocator)
{
}

inline std::shared_ptr<Buffer> BufferAllocations<void>::AllocateBuffer(size_t size) const
{
    return RTC::AllocateBuffer(size, GetAllocator());
}

inline std::shared_ptr<Buffer> BufferAllocations<void>::AllocateBuffer(size_t size,
                                                                       const void* data) const
{
    return RTC::AllocateBuffer(size, data, GetAllocator());
}

inline std::shared_ptr<Buffer> BufferAllocations<void>::AllocateBuffer(size_t size,
                                                                       const void* data,
                                                                       size_t dataSize) const
{
    return RTC::AllocateBuffer(size, data, dataSize, GetAllocator());
}

} // namespace RTC
