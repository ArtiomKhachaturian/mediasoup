#define MS_CLASS "RTC::SegmentsBuffer"
#include "RTC/Buffers/SegmentsBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

SegmentsBuffer::SegmentsBuffer(size_t capacity)
    : SegmentsBuffer(nullptr, capacity)
{
}

SegmentsBuffer::SegmentsBuffer(const std::shared_ptr<BufferAllocator>& allocator)
    : SegmentsBuffer(allocator, std::numeric_limits<size_t>::max())
{
}

SegmentsBuffer::SegmentsBuffer(const std::shared_ptr<BufferAllocator>& allocator, size_t capacity)
    : BufferAllocations<Buffer>(allocator)
    , _capacity(capacity)
{
    MS_ASSERT(_capacity, "capacity should be greater than zero");
}

SegmentsBuffer::Result SegmentsBuffer::Push(const std::shared_ptr<Buffer>& buffer)
{
    Result result = Result::Failed;
    if (buffer && !buffer->IsEmpty()) {
        MS_ASSERT(buffer.get() != this, "passed buffer is this instance");
        const auto bufferSize = buffer->GetSize();
        if (bufferSize <= GetCapacity()) {
            if (bufferSize + _size > GetCapacity()) {
                while(bufferSize + _size > GetCapacity()) {
                    _size -= _buffers.front()->GetSize();
                    _buffers.pop_front();
                }
                _buffers.push_front(buffer);
                result = Result::Front;
            }
            else {
                _buffers.push_back(buffer);
                result = Result::Back;
            }
            _size += bufferSize;
        }
        else {
            MS_ERROR_STD("pushed buffer size [%zu bytes] is greater capacity [%zu bytes]",
                         bufferSize, GetCapacity());
        }
    }
    return result;
}

void SegmentsBuffer::Clear()
{
    _buffers.clear();
    _size = 0UL;
}

auto SegmentsBuffer::GetBuffer(size_t& offset) const
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

size_t SegmentsBuffer::CopyTo(size_t offset, size_t len, uint8_t* output) const
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

void SegmentsBuffer::Merge()
{
    if (const auto count = _buffers.size()) {
        if (count > 1U) {
            auto merged = AllocateBuffer(_size);
            MS_ASSERT(merged, "failed to create merged buffer");
            MS_ASSERT(_size == merged->GetSize(), "merged size is incorrect");
            size_t offset = 0U;
            for (const auto& buffer : _buffers) {
                const auto size = buffer->GetSize();
                std::memcpy(merged->GetData() + offset, buffer->GetData(), size);
                offset += size;
            }
            _buffers.clear();
            _buffers.push_back(std::move(merged));
        }
    }
}

uint8_t* SegmentsBuffer::Merged() const
{
    if (!_buffers.empty()) {
        const_cast<SegmentsBuffer*>(this)->Merge();
        return _buffers.front()->GetData();
    }
    return nullptr;
}

} // namespace RTC
