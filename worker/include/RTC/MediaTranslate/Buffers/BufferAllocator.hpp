#pragma once
#include "RTC/MediaTranslate/Buffers/Buffer.hpp"

namespace RTC
{

class BufferAllocator
{
public:
	virtual ~BufferAllocator() = default;
    virtual std::shared_ptr<Buffer> Allocate(size_t size);
    std::shared_ptr<Buffer> Allocate(size_t size, const uint8_t* data);
    std::shared_ptr<Buffer> Allocate(size_t size, const uint8_t* data, size_t dataSize);
};

// helper routines
std::shared_ptr<Buffer> AllocateBuffer(size_t size,
                                       const std::weak_ptr<BufferAllocator>& allocator = std::weak_ptr<BufferAllocator>());
std::shared_ptr<Buffer> AllocateBuffer(size_t size, const uint8_t* data,
                                       const std::weak_ptr<BufferAllocator>& allocator = std::weak_ptr<BufferAllocator>());
std::shared_ptr<Buffer> AllocateBuffer(size_t size, const uint8_t* data, size_t dataSize,
                                       const std::weak_ptr<BufferAllocator>& allocator = std::weak_ptr<BufferAllocator>());

} // namespace RTC
