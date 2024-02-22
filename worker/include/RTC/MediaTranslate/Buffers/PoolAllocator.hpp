#pragma once
#include "RTC/MediaTranslate/Buffers/BufferAllocator.hpp"

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
    std::shared_ptr<Buffer> Allocate(size_t size) final;
    void PurgeGarbage() final;
private:
	const std::unique_ptr<AllocatorImpl> _impl;
};

} // namespace RTC
