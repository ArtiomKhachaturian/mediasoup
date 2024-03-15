#pragma once
#include "RTC/Buffers/PoolMemoryChunk.hpp"
#include <array>

namespace RTC
{

template<size_t size>
class PoolStackChunk : public PoolMemoryChunk
{
public:
    PoolStackChunk() = default;
    // impl. of MemoryChunk
    size_t GetSize() const final { return size; }
    uint8_t* GetData() final { return _memory.data(); }
    const uint8_t* GetData() const final { return _memory.data(); }
private:
    alignas(size) std::array<uint8_t, size> _memory;
};

template<>
class PoolStackChunk<1U> : public PoolMemoryChunk
{
public:
    PoolStackChunk<1U>() = default;
    // impl. of MemoryChunk
    size_t GetSize() const final { return 1U; }
    uint8_t* GetData() final { return &_byte; }
    const uint8_t* GetData() const final { return &_byte; }
private:
    uint8_t _byte;
};

} // namespace RTC