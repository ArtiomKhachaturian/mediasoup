#pragma once
#include "MemoryBuffer.hpp"
#include <list>
#include <limits>

namespace RTC
{

class CompoundMemoryBuffer : public MemoryBuffer
{
    using BuffersList = std::list<std::shared_ptr<MemoryBuffer>>;
public:
    CompoundMemoryBuffer(uint64_t capacity = std::numeric_limits<uint64_t>::max());
    bool Add(std::vector<uint8_t> data);
    bool Add(const void* buf, size_t len, const std::allocator<uint8_t>& allocator = {});
    bool Add(const std::shared_ptr<MemoryBuffer>& buffer);
    void Clear();
    size_t GetData(uint64_t offset, size_t len, uint8_t* output) const;
    uint64_t GetCapacity() const { return _capacity; }
    // impl. of MemoryBuffer
    uint64_t GetSize() const final { return _size; }
    uint8_t* GetData() final;
    const uint8_t* GetData() const final;
private:
    BuffersList::const_iterator GetBuffer(uint64_t& offset) const;
    // expensive operation
    uint8_t* Merge() const;
private:
    const size_t _capacity;
    BuffersList _buffers;
    uint64_t _size = 0UL;
    mutable std::unique_ptr<MemoryBuffer> _merged;
};

} // namespace RTC
