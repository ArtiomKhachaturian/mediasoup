#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"

namespace RTC
{

SimpleMemoryBuffer::SimpleMemoryBuffer(std::vector<uint8_t> buffer)
    : _buffer(std::move(buffer))
{
}

bool SimpleMemoryBuffer::Append(const void* buf, size_t len)
{
    return Insert(_buffer.end(), buf, len);
}

bool SimpleMemoryBuffer::Append(const MemoryBuffer& buffer)
{
    return Append(buffer.GetData(), buffer.GetSize());
}

bool SimpleMemoryBuffer::Prepend(const void* buf, size_t len)
{
    return Insert(_buffer.begin(), buf, len);
}

bool SimpleMemoryBuffer::Prepend(const MemoryBuffer& buffer)
{
    return Prepend(buffer.GetData(), buffer.GetSize());
}

void SimpleMemoryBuffer::Clear()
{
    _buffer.clear();
}

std::shared_ptr<SimpleMemoryBuffer> SimpleMemoryBuffer::Detach() const
{
    return Create(GetData(), GetSize());
}

std::shared_ptr<SimpleMemoryBuffer> SimpleMemoryBuffer::Take()
{
    return Create(TakeData());
}

std::shared_ptr<SimpleMemoryBuffer> SimpleMemoryBuffer::Create(const void* data, size_t len,
                                                               const std::allocator<uint8_t>& allocator)
{
    if (data && len) {
        const auto bytes = reinterpret_cast<const uint8_t*>(data);
        return Create(std::vector<uint8_t>(bytes, bytes + len, allocator));
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

bool SimpleMemoryBuffer::Insert(const std::vector<uint8_t>::iterator& where,
                                const void* buf, size_t len)
{
    if (buf && len) {
        const auto bytes = reinterpret_cast<const uint8_t*>(buf);
        _buffer.insert(where, bytes, bytes + len);
        return true;
    }
    return false;
}

} // namespace RTC
