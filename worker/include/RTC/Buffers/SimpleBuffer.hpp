#pragma once

#include "RTC/Buffers/Buffer.hpp"
#include <vector>
#include <memory>

namespace RTC
{

class SimpleBuffer : public Buffer
{
public:
    explicit SimpleBuffer(size_t capacity);
    SimpleBuffer(std::vector<uint8_t> buffer = {});
    SimpleBuffer(const void* buf, size_t len);
    bool Append(const void* buf, size_t len);
    bool Append(const std::shared_ptr<Buffer>& buffer);
    bool Prepend(const void* buf, size_t len);
    bool Prepend(const std::shared_ptr<Buffer>& buffer);
    void Clear();
    void Reserve(size_t size) { _buffer.reserve(size); }
    bool Resize(size_t size);
    std::vector<uint8_t> TakeData() { return std::move(_buffer); }
    size_t GetCapacity() const { return _buffer.capacity(); }
    size_t CopyTo(size_t offset, size_t len, uint8_t* output) const;
    // return a deep copy
    std::shared_ptr<SimpleBuffer> Detach() const;
    // return null if this buffer is empty
    std::shared_ptr<SimpleBuffer> Take();
    bool IsEmpty() const { return _buffer.empty(); }
    // create buffer with deep copy of input [data]
    static std::shared_ptr<SimpleBuffer> Create(const void* data, size_t len);
    static std::shared_ptr<SimpleBuffer> Create(std::vector<uint8_t> buffer);
    static std::shared_ptr<SimpleBuffer> Allocate(size_t capacity, size_t size = 0U);
    // impl. of Buffer
    size_t GetSize() const final { return _buffer.size(); }
    uint8_t* GetData() { return _buffer.data(); }
    const uint8_t* GetData() const { return _buffer.data(); }
private:
    bool Insert(const std::vector<uint8_t>::iterator& where, const void* buf, size_t len);
private:
    std::vector<uint8_t> _buffer;
};

} // namespace RTC
