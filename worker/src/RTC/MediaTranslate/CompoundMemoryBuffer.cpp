#define MS_CLASS "RTC::CompoundMemoryBuffer"
#include "RTC/MediaTranslate/CompoundMemoryBuffer.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

CompoundMemoryBuffer::CompoundMemoryBuffer()
{
}

bool CompoundMemoryBuffer:: Add(std::vector<uint8_t> data)
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
        _buffers.push_back(buffer);
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

uint8_t* CompoundMemoryBuffer::GetData()
{
    return Merge();
}

const uint8_t* CompoundMemoryBuffer::GetData() const
{
    return Merge();
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
