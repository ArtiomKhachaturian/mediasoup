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
    virtual uint8_t* GetData() = 0;
    virtual const uint8_t* GetData() const = 0;
    bool IsEmpty() const { return 0UL == GetSize() || nullptr == GetData() ; }
};

} // namespace RTC
