#pragma once
#include <atomic>

namespace RTC
{

class PoolMemoryChunk
{
public:
    virtual ~PoolMemoryChunk() = default;
    virtual size_t GetSize() const = 0;
    virtual uint8_t* GetData() = 0;
    virtual const uint8_t* GetData() const = 0;
    bool IsAcquired() const { return _acquired.load(); }
    bool Acquire();
    void Release();
protected:
    PoolMemoryChunk() = default;
    virtual void OnAcquired() {}
    virtual void OnReleased() {}
private:
    std::atomic_bool _acquired = false;
};

} // namespace RTC
