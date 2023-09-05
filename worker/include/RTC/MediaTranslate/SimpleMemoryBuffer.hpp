#pragma once

#include "MemoryBuffer.hpp"
#include <vector>
#include <memory>

namespace RTC
{

class SimpleMemoryBuffer : public MemoryBuffer
{
public:
    SimpleMemoryBuffer(std::vector<uint8_t> buffer);
    // create buffer with deep copy of input [data]
    static std::shared_ptr<MemoryBuffer> Create(const uint8_t* data, size_t len,
                                                const std::allocator<uint8_t>& allocator = {});
    // impl. of MemoryBuffer
    size_t GetSize() const final { return _buffer.size(); }
    uint8_t* GetData() { return _buffer.data(); }
    const uint8_t* GetData() const { return _buffer.data(); }
private:
    std::vector<uint8_t> _buffer;
};

} // namespace RTC
