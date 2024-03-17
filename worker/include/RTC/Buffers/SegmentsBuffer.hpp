#pragma once

#include "RTC/Buffers/BufferAllocations.hpp"
#include <list>
#include <limits>
#include <memory>

namespace RTC
{

class BufferAllocator;

class SegmentsBuffer : public BufferAllocations<Buffer>
{
    using BuffersList = std::list<std::shared_ptr<Buffer>>;
public:
    enum Result {
        Failed = 0, // error
        Front,      // added to beginning of segments list
        Back        // added to the end of segments list
    };
public:
    // capacity in bytes
    SegmentsBuffer(size_t capacity = std::numeric_limits<size_t>::max());
    SegmentsBuffer(const std::shared_ptr<BufferAllocator>& allocator);
    SegmentsBuffer(const std::shared_ptr<BufferAllocator>& allocator, size_t capacity);
    SegmentsBuffer(const SegmentsBuffer& other);
    SegmentsBuffer(SegmentsBuffer&& tmp);
    SegmentsBuffer& operator = (const SegmentsBuffer&) = delete;
    SegmentsBuffer& operator = (SegmentsBuffer&&) = delete;
    Result Push(std::shared_ptr<Buffer> buffer);
    std::shared_ptr<Buffer> Take();
    void Clear();
    size_t CopyTo(size_t offset, size_t len, uint8_t* output) const;
    size_t GetCapacity() const { return _capacity; }
    // expensive operation
    void Merge();
    // impl. of Buffer
    size_t GetSize() const final { return _size; }
    uint8_t* GetData() final { return Merged(); }
    const uint8_t* GetData() const final { return Merged(); }
private:
    auto GetBuffer(size_t& offset) const;
    // expensive operation
    uint8_t* Merged() const;
private:
    const size_t _capacity;
    mutable BuffersList _buffers;
    size_t _size = 0UL;
};

} // namespace RTC
