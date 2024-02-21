#include "RTC/MediaTranslate/Buffers/SimpleBuffer.hpp"
#include <cstring>

namespace RTC
{

SimpleBuffer::SimpleBuffer(size_t capacity)
{
    Reserve(capacity);
}

SimpleBuffer::SimpleBuffer(std::vector<uint8_t> buffer)
    : _buffer(std::move(buffer))
{
}

SimpleBuffer::SimpleBuffer(const void* buf, size_t len)
{
    if (buf && len) {
        Insert(_buffer.begin(), buf, len);
    }
}

bool SimpleBuffer::Append(const void* buf, size_t len)
{
    return Insert(_buffer.end(), buf, len);
}

bool SimpleBuffer::Append(const std::shared_ptr<MemoryBuffer>& buffer)
{
    return buffer && Append(buffer->GetData(), buffer->GetSize());
}

bool SimpleBuffer::Prepend(const void* buf, size_t len)
{
    return Insert(_buffer.begin(), buf, len);
}

bool SimpleBuffer::Prepend(const std::shared_ptr<MemoryBuffer>& buffer)
{
    return buffer && Prepend(buffer->GetData(), buffer->GetSize());
}

void SimpleBuffer::Clear()
{
    _buffer.clear();
}

size_t SimpleBuffer::CopyTo(size_t offset, size_t len, uint8_t* output) const
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

std::shared_ptr<SimpleBuffer> SimpleBuffer::Detach() const
{
    return Create(GetData(), GetSize());
}

std::shared_ptr<SimpleBuffer> SimpleBuffer::Take()
{
    return Create(TakeData());
}

std::shared_ptr<SimpleBuffer> SimpleBuffer::Create(const void* data, size_t len)
{
    const auto buffer = Allocate(len);
    if (buffer) {
        buffer->Append(data, len);
    }
    return buffer;
}

std::shared_ptr<SimpleBuffer> SimpleBuffer::Create(std::vector<uint8_t> buffer)
{
    return MakeMemoryBuffer<SimpleBuffer>(std::move(buffer));
}

std::shared_ptr<SimpleBuffer> SimpleBuffer::Allocate(size_t capacity, size_t size)
{
    auto buffer = MakeMemoryBuffer<SimpleBuffer>(capacity);
    buffer->Resize(size);
    return buffer;
}

bool SimpleBuffer::Insert(const std::vector<uint8_t>::iterator& where, const void* buf, size_t len)
{
    if (buf && len) {
        const auto bytes = reinterpret_cast<const uint8_t*>(buf);
        _buffer.insert(where, bytes, bytes + len);
        return true;
    }
    return false;
}

} // namespace RTC
