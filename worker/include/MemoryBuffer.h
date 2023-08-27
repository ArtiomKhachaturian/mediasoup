#ifndef MS_MEMORY_BUFFER_HPP
#define MS_MEMORY_BUFFER_HPP

#include <cstdint>
#include <cstddef>

class MemoryBuffer 
{
public:
    virtual ~MemoryBuffer() = default;
    virtual size_t GetSize() const = 0;
    virtual uint8_t* GetData() = 0;
    virtual const uint8_t* GetData() const = 0;
};

#endif
