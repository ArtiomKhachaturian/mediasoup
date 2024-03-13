#pragma once
#include "RTC/Buffers/BufferAllocator.hpp"
#include "handles/TimerHandle.hpp"
#include <array>
#include <list>
#include <map>
#include <shared_mutex>

namespace RTC
{

class PoolAllocator : public BufferAllocator, private TimerHandle::Listener
{
    class MemoryChunk;
    template<size_t size> class StackChunk;
    template<> class StackChunk<1U>;
    class HeapChunk;
    class RegularHeapChunk;
    template<size_t size> class PseudoHeapChunk;
	class BufferImpl;
    template<size_t count>
    using StackChunks = std::array<std::shared_ptr<MemoryChunk>, count>;
    // key is allocated size
    using HeapChunksMap = std::multimap<size_t, std::shared_ptr<HeapChunk>>;
    using MutexType = std::shared_mutex;
    using WriteMutexLock = std::unique_lock<MutexType>;
    using ReadMutexLock = std::shared_lock<MutexType>;
public:
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&) = delete;
	PoolAllocator();
	~PoolAllocator() final;
    PoolAllocator& operator = (const PoolAllocator&) = delete;
    PoolAllocator& operator = (PoolAllocator&&) = delete;
    // overrides of BufferAllocator
    bool RunGarbageCollector() final;
    void PurgeGarbage(uint32_t maxBufferAgeMs) final;
    void StopGarbageCollector() final;
protected:
    // overrides of BufferAllocator
    std::shared_ptr<Buffer> AllocateAligned(size_t size, size_t alignedSize) final;
private:
    template<size_t count, size_t size>
    static StackChunks<count> FillStackChunks();
    static size_t GetCachelineSize(size_t alignedSize, size_t cachelineTop);
    template<class TSerialChunks>
    static std::shared_ptr<MemoryChunk> GetAcquired(const TSerialChunks& chunks);
    static std::shared_ptr<MemoryChunk> GetAcquired(size_t size, const HeapChunksMap& chunks);
    static std::shared_ptr<MemoryChunk> GetAcquired(HeapChunksMap::const_iterator from,
                                                    HeapChunksMap::const_iterator to);
    static std::shared_ptr<HeapChunk> CreateHeapChunk(size_t alignedSize);
    std::shared_ptr<MemoryChunk> GetStackChunk(size_t alignedSize) const;
    std::shared_ptr<MemoryChunk> AcquireHeapChunk(size_t alignedSize);
    // impl. of TimerHandle::Listener
    void OnTimer(TimerHandle* timer) final;
private:
    // for replacement of small heap allocations (by performance reason)
    static inline constexpr size_t _maxEffectiveStackChunkSize = 16U;
    static inline constexpr size_t _maxStackChunkSize = 4096U;
    const StackChunks<32U> _stackChunks1byte;
    const StackChunks<32U> _stackChunks2bytes;
    const StackChunks<32U> _stackChunks4bytes;
    const StackChunks<32U> _stackChunks8bytes;
    const StackChunks<32U> _stackChunks16bytes;
    const StackChunks<32U> _stackChunks32bytes;
    const StackChunks<32U> _stackChunks64bytes;
    const StackChunks<32U> _stackChunks128bytes;
    const StackChunks<32U> _stackChunks256bytes;
    const StackChunks<32U> _stackChunks512bytes;
    // 1 & 2 kb chunks is most popular
    const StackChunks<64U> _stackChunks1024bytes;
    const StackChunks<128U> _stackChunks2048bytes;
    const StackChunks<32U> _stackChunks4096bytes;
    MutexType _heapChunksMtx;
    std::unique_ptr<TimerHandle> _garbageCollectorTimer;
    HeapChunksMap _heapChunks;
};

} // namespace RTC
