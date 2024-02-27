#define MS_CLASS "RTC::PoolAllocator"
#include "RTC/Buffers/PoolAllocator.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include "Logger.hpp"
#include "handles/TimerHandle.hpp"
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
#include "DepLibUV.hpp"
#include <mutex>
#include <shared_mutex>
#endif
#include <array>
#include <atomic>
#include <list>
#include <map>

namespace {

class MemoryChunk
{
public:
    virtual ~MemoryChunk() = default;
    virtual size_t GetSize() const = 0;
    virtual uint8_t* GetData() = 0;
    virtual const uint8_t* GetData() const = 0;
    bool IsAcquired() const { return _acquired.load(); }
    bool Acquire();
    void Release();
protected:
    MemoryChunk() = default;
    virtual void OnAcquired() {}
    virtual void OnReleased() {}
private:
    std::atomic_bool _acquired = false;
};

template<size_t size>
class StackChunk : public MemoryChunk
{
public:
    StackChunk() = default;
    // impl. of MemoryChunk
    size_t GetSize() const final { return size; }
    uint8_t* GetData() final { return _memory.data(); }
    const uint8_t* GetData() const final { return _memory.data(); }
private:
    alignas(size) std::array<uint8_t, size> _memory;
};

template<>
class StackChunk<1U> : public MemoryChunk
{
public:
    StackChunk<1U>() = default;
    // impl. of MemoryChunk
    size_t GetSize() const final { return 1U; }
    uint8_t* GetData() final { return &_byte; }
    const uint8_t* GetData() const final { return &_byte; }
private:
    uint8_t _byte;
};

#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
class HeapChunk : public MemoryChunk
{
public:
    // in milliseconds
    uint64_t GetAge() const;
protected:
    HeapChunk() = default;
    // overrides of MemoryChunk
    void OnAcquired() final;
    void OnReleased() final;
private:
    std::atomic<uint64_t> _lastReleaseTime = 0ULL;
};

class RegularHeapChunk : public HeapChunk
{
public:
    RegularHeapChunk(uint8_t* memory, size_t size);
    ~RegularHeapChunk() final;
    // impl. of MemoryChunk
    size_t GetSize() const final { return _size; }
    uint8_t* GetData() final { return _memory; }
    const uint8_t* GetData() const final { return _memory; }
private:
    uint8_t* const _memory;
    const size_t _size;
};

template<size_t size>
class PseudoHeapChunk : public HeapChunk
{
public:
    PseudoHeapChunk() = default;
    // impl. of MemoryChunk
    size_t GetSize() const final { return size; }
    uint8_t* GetData() final { return _memory.data(); }
    const uint8_t* GetData() const final { return _memory.data(); }
private:
    alignas(size) std::array<uint8_t, size> _memory;
};


using HeapChunkPtr = std::shared_ptr<HeapChunk>;
using HeapChunksMap = std::multimap<size_t, HeapChunkPtr>;
#endif

using MemoryChunkPtr = std::shared_ptr<MemoryChunk>;
using MemoryChunks = std::list<MemoryChunkPtr>;
using StackChunksMap = std::map<size_t, MemoryChunks>;

}

namespace RTC
{

class PoolAllocator::BufferImpl : public Buffer
{
public:
    BufferImpl(MemoryChunkPtr chunk, size_t size);
    ~BufferImpl() final;
    // impl. of Buffer
    size_t GetSize() const final { return _size.load(); }
    const uint8_t* GetData() const final;
    uint8_t* GetData() final;
    bool Resize(size_t size) final;
private:
    const MemoryChunkPtr _chunk;
    std::atomic<size_t> _size;
};

class PoolAllocator::AllocatorImpl : public TimerHandle::Listener
{
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    using MutexType = std::shared_mutex;
    using WriteMutexLock = std::unique_lock<MutexType>;
    using ReadMutexLock = std::shared_lock<MutexType>;
#endif
public:
    AllocatorImpl();
    ~AllocatorImpl();
    MemoryChunkPtr Allocate(size_t alignedSize);
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    bool RunGarbageCollector();
    void PurgeGarbage(uint32_t maxBufferAgeMs);
    void StopGarbageCollector();
#endif
private:
    MemoryChunkPtr GetStackChunk(size_t alignedSize) const;
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    MemoryChunkPtr AcquireHeapChunk(size_t alignedSize);
    static HeapChunkPtr CreateHeapChunk(size_t alignedSize);
    static MemoryChunkPtr GetAcquiredExactSize(size_t size, const HeapChunksMap& chunks);
    static MemoryChunkPtr GetAcquired(HeapChunksMap::const_iterator from,
                                      HeapChunksMap::const_iterator to);
#endif
    static MemoryChunkPtr GetAcquired(const MemoryChunks& chunks);
    template<size_t count, size_t size>
    static void FillStackChunks(StackChunksMap& chunks);
    static StackChunksMap CreateStackChunks();
    static size_t GetCachelineSize(size_t alignedSize, size_t cachelineTop);
    // impl. of TimerHandle::Listener
    void OnTimer(TimerHandle* timer) final;
private:
    // for replacement of small heap allocations (by performance reason)
    static inline constexpr size_t _maxEffectiveStackChunkSize = 16U;
    static inline constexpr size_t _maxStackChunkSize = 4096U;
    // key is allocated size
    const StackChunksMap _stackChunks;
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    MutexType _heapChunksMtx;
    std::unique_ptr<TimerHandle> _garbageCollectorTimer;
    HeapChunksMap _heapChunks;
#endif
};

PoolAllocator::PoolAllocator()
    : _impl(std::make_unique<AllocatorImpl>())
{
}

PoolAllocator::~PoolAllocator()
{
}

bool PoolAllocator::RunGarbageCollector()
{
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    return _impl->RunGarbageCollector();
#endif
    return BufferAllocator::RunGarbageCollector();
}

void PoolAllocator::PurgeGarbage(uint32_t maxBufferAgeMs)
{
    BufferAllocator::PurgeGarbage(maxBufferAgeMs);
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    _impl->PurgeGarbage(maxBufferAgeMs);
#endif
}

void PoolAllocator::StopGarbageCollector()
{
    BufferAllocator::StopGarbageCollector();
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    _impl->StopGarbageCollector();
#endif
}

std::shared_ptr<Buffer> PoolAllocator::AllocateAligned(size_t size, size_t alignedSize)
{
    if (size) {
        if (auto chunk = _impl->Allocate(alignedSize)) {
            return std::make_shared<BufferImpl>(std::move(chunk), size);
        }
    }
    return BufferAllocator::AllocateAligned(size, alignedSize);
}

PoolAllocator::BufferImpl::BufferImpl(MemoryChunkPtr chunk, size_t size)
    : _chunk(chunk)
    , _size(size)
{
}

PoolAllocator::BufferImpl::~BufferImpl()
{
    _chunk->Release();
}

const uint8_t* PoolAllocator::BufferImpl::GetData() const
{
    return IsEmpty() ? nullptr : _chunk->GetData();
}

uint8_t* PoolAllocator::BufferImpl::GetData()
{
    return IsEmpty() ? nullptr : _chunk->GetData();
}

bool PoolAllocator::BufferImpl::Resize(size_t size)
{
    if (size <= _chunk->GetSize() && _chunk->IsAcquired()) {
        _size = size;
        return true;
    }
    return false;
}

PoolAllocator::AllocatorImpl::AllocatorImpl()
    : _stackChunks(CreateStackChunks())
{
}

PoolAllocator::AllocatorImpl::~AllocatorImpl()
{
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    PurgeGarbage(0U);
#endif
}

MemoryChunkPtr PoolAllocator::AllocatorImpl::Allocate(size_t alignedSize)
{
    if (alignedSize) {
        auto chunk = GetStackChunk(alignedSize);
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
        if (!chunk) {
            chunk = AcquireHeapChunk(alignedSize);
        }
#endif
        return chunk;
    }
    return nullptr;
}

#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
bool PoolAllocator::AllocatorImpl::RunGarbageCollector()
{
    const WriteMutexLock lock(_heapChunksMtx);
    if (!_garbageCollectorTimer) {
        if (DepLibUV::GetLoop()) {
            const auto intervalMs = POOL_MEMORY_ALLOCATOR_HEAP_CHUNKS_LIFETIME_MS;
            _garbageCollectorTimer = std::make_unique<TimerHandle>(this);
            _garbageCollectorTimer->Start(intervalMs, intervalMs);
        }
    }
    return nullptr != _garbageCollectorTimer;
}

void PoolAllocator::AllocatorImpl::PurgeGarbage(uint32_t maxBufferAgeMs)
{
    size_t deallocated = 0U, still = 0U;
    {
        const WriteMutexLock lock(_heapChunksMtx);
        if (maxBufferAgeMs) {
            // TODO: change to https://en.cppreference.com/w/cpp/container/multimap/erase_if on C++20
            for (auto it = _heapChunks.begin(); it != _heapChunks.end();) {
                if (it->second->GetAge() >= maxBufferAgeMs) {
                    it = _heapChunks.erase(it);
                    ++deallocated;
                }
                else {
                    ++it;
                    ++still;
                }
            }
        }
        else {
            _heapChunks.clear();
        }
    }
    if (deallocated || still) {
        float usage = 100.f;
        if (deallocated) {
            usage = std::floor(100 * (float(still) / float(still + deallocated)));
        }
        MS_DUMP_STD("Purge stats: deallocated chunks %zu, still %zu, usage %d %%",
                     deallocated, still, static_cast<int>(usage));
    }
}

void PoolAllocator::AllocatorImpl::StopGarbageCollector()
{
    const WriteMutexLock lock(_heapChunksMtx);
    _garbageCollectorTimer.reset();
}
#endif

MemoryChunkPtr PoolAllocator::AllocatorImpl::GetStackChunk(size_t alignedSize) const
{
    MemoryChunkPtr chunk;
    if (alignedSize && alignedSize <= _maxStackChunkSize) {
        // trying exact shooting to cached chunks
        if (alignedSize > 1U && alignedSize < _maxStackChunkSize) {
            // find appropriate cache line for avoiding of cache miss
            alignedSize = GetCachelineSize(alignedSize, _maxStackChunkSize);
        }
        // switch to next cache lines non-effective from mem usage point of view,
        // only exact match
        auto itc = _stackChunks.find(alignedSize);
        if (itc != _stackChunks.end()) {
            chunk = GetAcquired(itc->second);
        }
    }
    return chunk;
}

#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
MemoryChunkPtr PoolAllocator::AllocatorImpl::AcquireHeapChunk(size_t alignedSize)
{
    MemoryChunkPtr chunk;
    {
        const ReadMutexLock lock(_heapChunksMtx);
        chunk = GetAcquiredExactSize(alignedSize, _heapChunks);
        if (!chunk) {
            chunk = GetAcquired(_heapChunks.upper_bound(alignedSize), _heapChunks.end());
        }
    }
    if (!chunk) {
        auto heapChunk = CreateHeapChunk(alignedSize);
        if (heapChunk && heapChunk->Acquire()) {
            {
                const WriteMutexLock lock(_heapChunksMtx);
                _heapChunks.insert({alignedSize, heapChunk});
                if (alignedSize != heapChunk->GetSize()) {
                    _heapChunks.insert({heapChunk->GetSize(), heapChunk});
                }
            }
            chunk = std::move(heapChunk);
        }
    }
    return chunk;
}

HeapChunkPtr PoolAllocator::AllocatorImpl::CreateHeapChunk(size_t alignedSize)
{
    HeapChunkPtr chunk;
    if (alignedSize) {
        if (alignedSize <= _maxEffectiveStackChunkSize) {
            switch (GetCachelineSize(alignedSize, _maxEffectiveStackChunkSize)) {
                case 1U:
                    chunk = std::make_shared<PseudoHeapChunk<1U>>();
                    break;
                case 2U:
                    chunk = std::make_shared<PseudoHeapChunk<2U>>();
                    break;
                case 4U:
                    chunk = std::make_shared<PseudoHeapChunk<4U>>();
                    break;
                case 8U:
                    chunk = std::make_shared<PseudoHeapChunk<8U>>();
                    break;
                case 16U:
                    chunk = std::make_shared<PseudoHeapChunk<16U>>();
                    break;
            }
        }
        if (!chunk) {
            if (auto memory = new (std::nothrow) uint8_t[alignedSize]) {
                chunk = std::make_shared<RegularHeapChunk>(memory, alignedSize);
            }
        }
    }
    return chunk;
}

MemoryChunkPtr PoolAllocator::AllocatorImpl::GetAcquiredExactSize(size_t size,
                                                                  const HeapChunksMap& chunks)
{
    if (size) {
        const auto exact = chunks.equal_range(size);
        return GetAcquired(exact.first, exact.second);
    }
    return nullptr;
}

MemoryChunkPtr PoolAllocator::AllocatorImpl::GetAcquired(HeapChunksMap::const_iterator from,
                                                         HeapChunksMap::const_iterator to)
{
    for (auto it = from; it != to; ++it) {
        if (it->second->Acquire()) {
            return it->second;
        }
    }
    return nullptr;
}
#endif

MemoryChunkPtr PoolAllocator::AllocatorImpl::GetAcquired(const MemoryChunks& chunks)
{
    for (const auto& chunk : chunks) {
        if (chunk->Acquire()) {
            return chunk;
        }
    }
    return nullptr;
}

template<size_t count, size_t size>
void PoolAllocator::AllocatorImpl::FillStackChunks(StackChunksMap& map)
{
    MemoryChunks chunks;
    for (size_t i = 0U; i < count; ++i) {
        chunks.push_back(std::make_shared<StackChunk<size>>());
    }
    map[size] = std::move(chunks);
}

StackChunksMap PoolAllocator::AllocatorImpl::CreateStackChunks()
{
    StackChunksMap stackChunks;
    //stackChunks.reserve(13U);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 1U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 2U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 4U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 8U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 16U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 32U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 64U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 128U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 256U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 512U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 1024U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 2048U>(stackChunks);
    FillStackChunks<PoolAllocator::_countOfStackBlocks, 4096U>(stackChunks);
    return stackChunks;
}

size_t PoolAllocator::AllocatorImpl::GetCachelineSize(size_t alignedSize, size_t cachelineTop)
{
    size_t target = cachelineTop;
    while (target) {
        if (1U == target / alignedSize) {
            alignedSize = target;
            break;
        }
        target = target >> 1;
    }
    return alignedSize;
}

void PoolAllocator::AllocatorImpl::OnTimer(TimerHandle* timer)
{
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    if (timer == _garbageCollectorTimer.get()) {
        PurgeGarbage(static_cast<uint32_t>(timer->GetTimeout()));
    }
#endif
}

} // namespace RTC

namespace {


bool MemoryChunk::Acquire()
{
    if (!_acquired.exchange(true)) {
        OnAcquired();
        return true;
    }
    return false;
}

void MemoryChunk::Release()
{
    if (_acquired.exchange(false)) {
        OnReleased();
    }
}

#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
uint64_t HeapChunk::GetAge() const
{
    if (const auto lastReleaseTime = _lastReleaseTime.load()) {
        return DepLibUV::GetTimeMs() - lastReleaseTime;
    }
    return 0ULL;
}

void HeapChunk::OnAcquired()
{
    MemoryChunk::OnAcquired();
    _lastReleaseTime = 0ULL;
}

void HeapChunk::OnReleased()
{
    MemoryChunk::OnReleased();
    _lastReleaseTime = DepLibUV::GetTimeMs();
}

RegularHeapChunk::RegularHeapChunk(uint8_t* memory, size_t size)
    : _memory(memory)
    , _size(size)
{
}

RegularHeapChunk::~RegularHeapChunk()
{
    delete [] _memory;
}
#endif

}
