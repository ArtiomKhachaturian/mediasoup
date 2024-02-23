#pragma once
#include "RTC/MediaTranslate/Buffers/Buffer.hpp"
#include <memory>

namespace RTC
{

class BufferAllocator
{
public:
	virtual ~BufferAllocator() = default;
    std::shared_ptr<Buffer> Allocate(size_t size);
    std::shared_ptr<Buffer> Allocate(size_t size, const void* data);
    std::shared_ptr<Buffer> Allocate(size_t size, const void* data, size_t dataSize);
    virtual void PurgeGarbage(uint32_t /*maxBufferAgeMs*/) {}
protected:
    virtual std::shared_ptr<Buffer> AllocateAligned(size_t size, size_t alignedSize);
};

// helper routines
size_t GetAlignedBufferSize(size_t size, size_t alignment = 2U);
std::shared_ptr<Buffer> AllocateBuffer(size_t size,
                                       const std::weak_ptr<BufferAllocator>& allocator = std::weak_ptr<BufferAllocator>());
std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data,
                                       const std::weak_ptr<BufferAllocator>& allocator = std::weak_ptr<BufferAllocator>());
std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data, size_t dataSize,
                                       const std::weak_ptr<BufferAllocator>& allocator = std::weak_ptr<BufferAllocator>());
std::shared_ptr<Buffer> ReallocateBuffer(size_t size, const std::shared_ptr<Buffer>& buffer,
                                         const std::weak_ptr<BufferAllocator>& allocator = std::weak_ptr<BufferAllocator>());

} // namespace RTC
