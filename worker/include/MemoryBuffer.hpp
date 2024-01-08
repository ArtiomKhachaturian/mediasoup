#pragma once

#include <cstdint>
#include <cstddef>

namespace RTC
{

class MemoryBuffer
{
public:
    virtual ~MemoryBuffer() = default;
    virtual size_t GetSize() const = 0;
    // maybe std::byte?
    virtual uint8_t* GetData() = 0;
    virtual const uint8_t* GetData() const = 0;
    bool IsEmpty() const { return nullptr == GetData() || 0UL == GetSize(); }
};

} // namespace RTC
