#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"

namespace RTC
{

SimpleMemoryBuffer::SimpleMemoryBuffer(std::vector<uint8_t> buffer)
    : _buffer(std::move(buffer))
{
}

bool SimpleMemoryBuffer::Append(const void* buf, size_t len)
{
    if (buf && len) {
        const auto bytes = reinterpret_cast<const uint8_t*>(buf);
        _buffer.insert(_buffer.end(), bytes, bytes + len);
        return true;
    }
    return false;
}

bool SimpleMemoryBuffer::Prepend(const void* buf, size_t len)
{
    if (buf && len) {
        const auto bytes = reinterpret_cast<const uint8_t*>(buf);
        _buffer.insert(_buffer.begin(), bytes, bytes + len);
        return true;
    }
    return false;
}

std::shared_ptr<SimpleMemoryBuffer> SimpleMemoryBuffer::Detach() const
{
    return Create(GetData(), GetSize());
}

std::shared_ptr<SimpleMemoryBuffer> SimpleMemoryBuffer::Take()
{
    return Create(TakeData());
}

std::shared_ptr<SimpleMemoryBuffer> SimpleMemoryBuffer::Create(const uint8_t* data, size_t len,
                                                               const std::allocator<uint8_t>& allocator)
{
    if (data && len) {
        return Create(std::vector<uint8_t>(data, data + len, allocator));
    }
    return nullptr;
}

std::shared_ptr<SimpleMemoryBuffer> SimpleMemoryBuffer::Create(std::vector<uint8_t> buffer)
{
    if (!buffer.empty()) {
        return std::make_shared<SimpleMemoryBuffer>(std::move(buffer));
    }
    return nullptr;
}

} // namespace RTC
