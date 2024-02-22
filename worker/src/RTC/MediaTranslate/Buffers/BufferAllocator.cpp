#define MS_CLASS "RTC::BufferAllocator"
#include "RTC/MediaTranslate/Buffers/BufferAllocator.hpp"
#include "RTC/MediaTranslate/Buffers/SimpleBuffer.hpp"
#include "Logger.hpp"

namespace {

using namespace RTC;

inline std::shared_ptr<Buffer> AllocateSimple(size_t size)
{
    const auto buffer = std::make_shared<SimpleBuffer>(size);
    buffer->Resize(size);
    return buffer;
}

inline void Copy(const std::shared_ptr<Buffer>& dst, const void* source, size_t size)
{
    if (dst && source && size) {
        MS_ASSERT(size <= dst->GetSize(), "size of source memory chunk is greater than buffer");
        std::memcpy(dst->GetData(), source, size);
    }
}

}

namespace RTC
{

std::shared_ptr<Buffer> BufferAllocator::Allocate(size_t size)
{
    return AllocateSimple(size);
}

std::shared_ptr<Buffer> BufferAllocator::Allocate(size_t size, const void* data)
{
    return Allocate(size, data, size);
}

std::shared_ptr<Buffer> BufferAllocator::Allocate(size_t size, const void* data,
                                                  size_t dataSize)
{
    const auto buffer = Allocate(size);
    Copy(buffer, data, dataSize);
    return buffer;
}

std::shared_ptr<Buffer> AllocateBuffer(size_t size, const std::weak_ptr<BufferAllocator>& allocatorRef)
{
    if (const auto allocator = allocatorRef.lock()) {
        return allocator->Allocate(size);
    }
    return AllocateSimple(size);
}

std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data,
                                       const std::weak_ptr<BufferAllocator>& allocatorRef)
{
    return AllocateBuffer(size, data, size, allocatorRef);
}

std::shared_ptr<Buffer> AllocateBuffer(size_t size, const void* data, size_t dataSize,
                                       const std::weak_ptr<BufferAllocator>& allocatorRef)
{
    std::shared_ptr<Buffer> buffer;
    if (const auto allocator = allocatorRef.lock()) {
        buffer = allocator->Allocate(size, data, dataSize);
    }
    else {
        buffer = AllocateSimple(size);
        Copy(buffer, data, dataSize);
    }
    return buffer;
}

std::shared_ptr<Buffer> ReallocateBuffer(size_t size, const std::shared_ptr<Buffer>& buffer,
                                         const std::weak_ptr<BufferAllocator>& allocatorRef)
{
    if (buffer) {
        const auto bufferSize = buffer->GetSize();
        if (bufferSize != size) {
            if (!buffer->Resize(size)) {
                const auto dataSize = std::min(size, bufferSize);
                return AllocateBuffer(size, buffer->GetData(), dataSize);
            }
        }
    }
    return buffer;
}


} // namespace RTC
