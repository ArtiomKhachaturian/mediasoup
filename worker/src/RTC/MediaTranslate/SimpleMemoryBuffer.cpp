#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"

namespace RTC
{

SimpleMemoryBuffer::SimpleMemoryBuffer(size_t capacity)
{
    Reserve(capacity);
}

SimpleMemoryBuffer::SimpleMemoryBuffer(std::vector<uint8_t> buffer)
    : _buffer(std::move(buffer))
{
}

bool SimpleMemoryBuffer::Append(const void* buf, size_t len)
{
    return Insert(_buffer.end(), buf, len);
}

bool SimpleMemoryBuffer::Append(const std::shared_ptr<MemoryBuffer>& buffer)
{
    return buffer && Append(buffer->GetData(), buffer->GetSize());
}

bool SimpleMemoryBuffer::Prepend(const void* buf, size_t len)
{
    return Insert(_buffer.begin(), buf, len);
}

bool SimpleMemoryBuffer::Prepend(const std::shared_ptr<MemoryBuffer>& buffer)
{
    return buffer && Prepend(buffer->GetData(), buffer->GetSize());
}

void SimpleMemoryBuffer::Clear()
{
    _buffer.clear();
}

size_t SimpleMemoryBuffer::GetData(size_t offset, size_t len, uint8_t* output) const
{
    if (len && output && offset < _buffer.size()) {
        len = std::min<size_t>(len, _buffer.size() - offset);
        if (len) {
            std::memcpy(output, _buffer.data() + offset, len);
            return len;
        }
    }
    return 0UL;
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
