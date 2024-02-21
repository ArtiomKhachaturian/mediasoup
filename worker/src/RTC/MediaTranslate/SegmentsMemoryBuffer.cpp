#define MS_CLASS "RTC::SegmentsMemoryBuffer"
#include "RTC/MediaTranslate/SegmentsMemoryBuffer.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

SegmentsMemoryBuffer::SegmentsMemoryBuffer(size_t capacity)
    : _capacity(capacity)
{
    MS_ASSERT(_capacity, "capacity should be greater than zero");
}

SegmentsMemoryBuffer::AppendResult SegmentsMemoryBuffer::Append(const std::shared_ptr<MemoryBuffer>& buffer)
{
    AppendResult result = AppendResult::Failed;
    if (buffer && !buffer->IsEmpty()) {
        MS_ASSERT(buffer.get() != this, "passed buffer is this instance");
        MS_ASSERT(buffer->GetSize() <= GetCapacity(), "buffer is too huge");
        if (buffer->GetSize() + _size > GetCapacity()) {
            while(buffer->GetSize() + _size > GetCapacity()) {
                _size -= _buffers.front()->GetSize();
                _buffers.pop_front();
            }
            _buffers.push_front(buffer);
            result = AppendResult::Front;
        }
        else {
            _buffers.push_back(buffer);
            result = AppendResult::Back;
        }
        _size += buffer->GetSize();
    }
    return result;
}

void SegmentsMemoryBuffer::Clear()
{
    _buffers.clear();
    _size = 0UL;
}

auto SegmentsMemoryBuffer::GetBuffer(size_t& offset) const
{
    if (!_buffers.empty()) {
        size_t current = 0ULL;
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

size_t SegmentsMemoryBuffer::CopyTo(size_t offset, size_t len, uint8_t* output) const
{
    size_t actual = 0UL;
    if (output && len) {
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

uint8_t* SegmentsMemoryBuffer::GetData()
{
    return Merge();
}

const uint8_t* SegmentsMemoryBuffer::GetData() const
{
    return Merge();
}

uint8_t* SegmentsMemoryBuffer::Merge() const
{
    if (const auto count = _buffers.size()) {
        if (count > 1U) {
            auto merged = MakeMemoryBuffer<SimpleMemoryBuffer>();
            merged->Reserve(_size);
            for (const auto& buffer : _buffers) {
                merged->Append(buffer);
            }
            MS_ASSERT(_size == merged->GetSize(), "merged size is incorrect");
            _buffers.clear();
            _buffers.push_back(std::move(merged));
        }
        return _buffers.front()->GetData();
    }
    return nullptr;
}

} // namespace RTC
