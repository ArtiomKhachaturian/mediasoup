#pragma once

#include "RTC/ObjectId.hpp"
#include <cstdint>
#include <cstddef>
#include <memory>

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
    bool IsEqual(const Buffer& other) const;
    bool IsEqual(const std::shared_ptr<const Buffer>& other) const;
    bool IsEmpty() const { return 0U == GetSize(); }
protected:
    Buffer() = default;
};

} // namespace RTC
