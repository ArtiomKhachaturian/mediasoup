#pragma once

#include "MemoryBuffer.hpp"
#include <vector>
#include <memory>

namespace RTC
{

class SimpleMemoryBuffer : public MemoryBuffer
{
public:
    SimpleMemoryBuffer(std::vector<uint8_t> buffer = {});
    bool Append(const void* buf, size_t len);
    bool Append(const MemoryBuffer& buffer);
    bool Prepend(const void* buf, size_t len);
    bool Prepend(const MemoryBuffer& buffer);
    void Reserve(size_t size) { _buffer.reserve(size); }
    void Resize(size_t size) { _buffer.resize(size); }
    std::vector<uint8_t> TakeData() { return std::move(_buffer); }
    // return a deep copy
    std::shared_ptr<SimpleMemoryBuffer> Detach() const;
    // return null if this buffer is empty
    std::shared_ptr<SimpleMemoryBuffer> Take();
    bool IsEmpty() const { return _buffer.empty(); }
    // create buffer with deep copy of input [data]
    static std::shared_ptr<SimpleMemoryBuffer> Create(const uint8_t* data, size_t len,
                                                      const std::allocator<uint8_t>& allocator = {});
    static std::shared_ptr<SimpleMemoryBuffer> Create(std::vector<uint8_t> buffer);
    // impl. of MemoryBuffer
    size_t GetSize() const final { return _buffer.size(); }
    uint8_t* GetData() { return _buffer.data(); }
    const uint8_t* GetData() const { return _buffer.data(); }
private:
    std::vector<uint8_t> _buffer;
};

} // namespace RTC
