#pragma once
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#include <list>
#include <limits>

namespace RTC
{

class SegmentsMemoryBuffer : public MemoryBuffer
{
    using BuffersList = std::list<std::shared_ptr<MemoryBuffer>>;
public:
    enum AppendResult {
        Failed = 0, // error
        Front,      // added to beginning of segments list
        Back        // added to the end of segments list
    };
public:
    // capacity in bytes
    SegmentsMemoryBuffer(size_t capacity = std::numeric_limits<size_t>::max());
    AppendResult Append(std::vector<uint8_t> data);
    AppendResult Append(const void* buf, size_t len, const std::allocator<uint8_t>& allocator = {});
    AppendResult Append(const std::shared_ptr<MemoryBuffer>& buffer);
    void Clear();
    size_t CopyTo(size_t offset, size_t len, uint8_t* output) const;
    size_t GetCapacity() const { return _capacity; }
    // impl. of MemoryBuffer
    size_t GetSize() const final { return _size; }
    uint8_t* GetData() final;
    const uint8_t* GetData() const final;
private:
    auto GetBuffer(size_t& offset) const;
    // expensive operation
    uint8_t* Merge() const;
private:
    const size_t _capacity;
    BuffersList _buffers;
    size_t _size = 0UL;
    mutable std::unique_ptr<MemoryBuffer> _merged;
};

} // namespace RTC
