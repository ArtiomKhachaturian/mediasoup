#pragma once
#include "MemoryBuffer.hpp"
#include <list>

namespace RTC
{

class CompoundMemoryBuffer : public MemoryBuffer
{
public:
    CompoundMemoryBuffer();
    bool Add(std::vector<uint8_t> data);
    bool Add(const void* buf, size_t len, const std::allocator<uint8_t>& allocator = {});
    bool Add(const std::shared_ptr<MemoryBuffer>& buffer);
    void Clear();
    // impl. of MemoryBuffer
    size_t GetSize() const final { return _size; }
    uint8_t* GetData() final;
    const uint8_t* GetData() const final;
private:
    // expensive operation
    uint8_t* Merge() const;
private:
    std::list<std::shared_ptr<MemoryBuffer>> _buffers;
    size_t _size = 0UL;
    mutable std::unique_ptr<MemoryBuffer> _merged;
};

} // namespace RTC
