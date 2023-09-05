#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"

namespace RTC
{

SimpleMemoryBuffer::SimpleMemoryBuffer(std::vector<uint8_t> buffer)
    : _buffer(std::move(buffer))
{
}

std::shared_ptr<MemoryBuffer> SimpleMemoryBuffer::Create(const uint8_t* data, size_t len,
                                                         const std::allocator<uint8_t>& allocator)
{
    if (data && len) {
        std::vector<uint8_t> buffer(data, data + len, allocator);
        return std::make_shared<SimpleMemoryBuffer>(std::move(buffer));
    }
    return nullptr;
}

} // namespace RTC
