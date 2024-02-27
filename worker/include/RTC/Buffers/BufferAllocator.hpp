#pragma once

#include "RTC/Buffers/Buffer.hpp"
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
    virtual void PurgeGarbage(uint32_t /*maxBufferAgeMs*/ = 0U) {}
protected:
    virtual std::shared_ptr<Buffer> AllocateAligned(size_t size, size_t alignedSize);
};

// helper routines
size_t GetAlignedBufferSize(size_t size, size_t alignment = 2U);
std::shared_ptr<Buffer> AllocateBuffer(size_t size,
                                       const std::shared_ptr<BufferAllocator>& allocator = nullptr);
std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data,
                                       const std::shared_ptr<BufferAllocator>& allocator = nullptr);
std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data, size_t dataSize,
                                       const std::shared_ptr<BufferAllocator>& allocator = nullptr);
std::shared_ptr<Buffer> ReallocateBuffer(size_t size, const std::shared_ptr<Buffer>& buffer,
                                         const std::shared_ptr<BufferAllocator>& allocator = nullptr);

} // namespace RTC
