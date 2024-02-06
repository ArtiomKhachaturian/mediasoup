#define MS_CLASS "RTC::CompoundMemoryBuffer"
#include "RTC/MediaTranslate/CompoundMemoryBuffer.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

CompoundMemoryBuffer::CompoundMemoryBuffer(uint64_t capacity)
    : _capacity(capacity)
{
    MS_ASSERT(_capacity, "capacity should be greater than zero");
}

bool CompoundMemoryBuffer::Add(std::vector<uint8_t> data)
{
    return Add(SimpleMemoryBuffer::Create(std::move(data)));
}

bool CompoundMemoryBuffer::Add(const void* buf, size_t len, const std::allocator<uint8_t>& allocator)
{
    return Add(SimpleMemoryBuffer::Create(buf, len, allocator));
}

bool CompoundMemoryBuffer::Add(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer && !buffer->IsEmpty()) {
        MS_ASSERT(buffer.get() != this, "passed buffer is this instance");
        MS_ASSERT(buffer->GetSize() <= _capacity, "buffer is too huge");
        if (buffer->GetSize() + _size > _capacity) {
            while(buffer->GetSize() + _size > _capacity) {
                _size -= _buffers.front()->GetSize();
                _buffers.pop_front();
            }
            _buffers.push_front(buffer);
        }
        else {
            _buffers.push_back(buffer);
        }
        _size += buffer->GetSize();
        _merged = nullptr;
        return true;
    }
    return false;
}

void CompoundMemoryBuffer::Clear()
{
    _buffers.clear();
    _size = 0UL;
    _merged.reset();
}

size_t CompoundMemoryBuffer::GetData(uint64_t offset, size_t len, uint8_t* output) const
{
    size_t actual = 0UL;
    if (output && len && offset + len <= GetSize()) {
        auto it = GetBuffer(offset);
        if (it != _buffers.end()) {
            for (; it != _buffers.end() && actual < len; ++it) {
                const auto size = std::min<size_t>(len - actual, it->get()->GetSize() - offset);
                std::memcpy(output, it->get()->GetData() + offset, size);
                actual += size;
                output += size;
                offset = 0ULL;
            }
        }
    }
    return actual;
}

uint8_t* CompoundMemoryBuffer::GetData()
{
    return Merge();
}

const uint8_t* CompoundMemoryBuffer::GetData() const
{
    return Merge();
}

CompoundMemoryBuffer::BuffersList::const_iterator CompoundMemoryBuffer::GetBuffer(uint64_t& offset) const
{
    if (!_buffers.empty()) {
        uint64_t current = 0ULL;
        for (auto it = _buffers.begin(); it != _buffers.end(); ++it) {
            const auto size = it->get()->GetSize();
            if (current + size >= offset) {
                offset -= current;
                return it;
            }
            current += size;
        }
    }
    return _buffers.end();
}

uint8_t* CompoundMemoryBuffer::Merge() const
{
    if (const auto count = _buffers.size()) {
        if (1UL == count) {
            return _buffers.front()->GetData();
        }
        if (!_merged) {
            auto merged = std::make_unique<SimpleMemoryBuffer>();
            merged->Reserve(_size);
            for (const auto& buffer : _buffers) {
                merged->Append(*buffer);
            }
            MS_ASSERT(_size == merged->GetSize(), "merged size is incorrect");
            _merged = std::move(merged);
        }
    }
    return _merged ? _merged->GetData() : nullptr;
}

} // namespace RTC
