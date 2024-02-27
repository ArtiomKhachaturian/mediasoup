#define MS_CLASS "RTC::BufferAllocator"
#include "RTC/Buffers/BufferAllocator.hpp"
#include "RTC/Buffers/SimpleBuffer.hpp"
#include "Logger.hpp"

namespace {

using namespace RTC;

inline std::shared_ptr<Buffer> AllocateSimple(size_t size, size_t alignedSize)
{
    const auto buffer = std::make_shared<SimpleBuffer>(alignedSize);
    buffer->Resize(size);
    return buffer;
}

inline std::shared_ptr<Buffer> Copy(std::shared_ptr<Buffer> dst,
                                    const void* source, size_t size)
{
    if (dst && source && size) {
        MS_ASSERT(size <= dst->GetSize(),
                  "source data size [%zu bytes] is greater than "
                  "target buffer size [%zu bytes]",
                  size, dst->GetSize());
        std::memcpy(dst->GetData(), source, size);
    }
    return dst;
}

}

namespace RTC
{

std::shared_ptr<Buffer> BufferAllocator::Allocate(size_t size)
{
    return AllocateAligned(size, GetAlignedBufferSize(size));
}

std::shared_ptr<Buffer> BufferAllocator::Allocate(size_t size, const void* data)
{
    return Allocate(size, data, size);
}

std::shared_ptr<Buffer> BufferAllocator::Allocate(size_t size, const void* data,
                                                  size_t dataSize)
{
    return Copy(Allocate(size), data, dataSize);
}

std::shared_ptr<Buffer> BufferAllocator::AllocateAligned(size_t size, size_t alignedSize)
{
    return AllocateSimple(size, alignedSize);
}

size_t GetAlignedBufferSize(size_t size, size_t alignment)
{
    // even alignment expected
    if (size && size > 1U && alignment < size && 0 == (alignment & (alignment - 1))) {
        if (const auto rest = size % alignment) {
            size = size + (alignment - rest);
        }
    }
    return size;
}

std::shared_ptr<Buffer> AllocateBuffer(size_t size, const std::shared_ptr<BufferAllocator>& allocator)
{
    return allocator ? allocator->Allocate(size) : AllocateSimple(size, GetAlignedBufferSize(size));
}

std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data,
                                       const std::shared_ptr<BufferAllocator>& allocator)
{
    return AllocateBuffer(size, data, size, allocator);
}

std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data, size_t dataSize,
                                       const std::shared_ptr<BufferAllocator>& allocator)
{
    std::shared_ptr<Buffer> buffer;
    if (allocator) {
        return allocator->Allocate(size, data, dataSize);
    }
    return Copy(AllocateSimple(size, GetAlignedBufferSize(size)), data, dataSize);
}

std::shared_ptr<Buffer> ReallocateBuffer(size_t size, const std::shared_ptr<Buffer>& buffer,
                                         const std::shared_ptr<BufferAllocator>& allocator)
{
    if (buffer) {
        const auto bufferSize = buffer->GetSize();
        if (bufferSize != size) {
            if (!buffer->Resize(size)) {
                const auto dataSize = std::min(size, bufferSize);
                return AllocateBuffer(size, buffer->GetData(), dataSize, allocator);
            }
        }
    }
    return buffer;
}


} // namespace RTC
