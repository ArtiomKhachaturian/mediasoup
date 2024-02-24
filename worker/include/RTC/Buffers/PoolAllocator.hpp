#pragma once

#include "RTC/Buffers/BufferAllocator.hpp"

namespace RTC
{

class PoolAllocator : public BufferAllocator
{
	class AllocatorImpl;
	class BufferImpl;
public:
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&) = delete;
	PoolAllocator();
	~PoolAllocator() final;
    PoolAllocator& operator = (const PoolAllocator&) = delete;
    PoolAllocator& operator = (PoolAllocator&&) = delete;
    // overrides of BufferAllocator
    void PurgeGarbage(uint32_t maxBufferAgeMs = 0U) final;
protected:
    std::shared_ptr<Buffer> AllocateAligned(size_t size, size_t alignedSize) final;
private:
    static inline constexpr size_t _countOfStackBlocks = 32U;
	const std::unique_ptr<AllocatorImpl> _impl;
};

} // namespace RTC
