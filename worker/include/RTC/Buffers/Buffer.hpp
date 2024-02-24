#pragma once

#include "RTC/ObjectId.hpp"
#include <cstdint>
#include <cstddef>

namespace RTC
{

class Buffer : public ObjectId
{
public:
    virtual ~Buffer() = default;
    virtual size_t GetSize() const = 0;
    virtual uint8_t* GetData() = 0;
    virtual const uint8_t* GetData() const = 0;
    virtual bool Resize(size_t /*size*/) { return false; }
    bool IsEmpty() const { return 0U == GetSize(); }
};

} // namespace RTC
