#define MS_CLASS "RTC::PoolAllocator"
#include "RTC/MediaTranslate/Buffers/PoolAllocator.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
#include "ProtectedObj.hpp"
#include "DepLibUV.hpp"
#endif
#include "Logger.hpp"
#include "handles/TimerHandle.hpp"
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
    HeapChunk(std::unique_ptr<uint8_t[]> memory, size_t size);
    // in milliseconds
    uint64_t GetAge() const;
    // impl. of MemoryChunk
    size_t GetSize() const final { return _size; }
    uint8_t* GetData() final { return _memory.get(); }
    const uint8_t* GetData() const final { return _memory.get(); }
protected:
    void OnAcquired() final;
    void OnReleased() final;
private:
    const std::unique_ptr<uint8_t[]> _memory;
    const size_t _size;
    std::atomic<uint64_t> _lastReleaseTime = 0ULL;
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
public:
    AllocatorImpl();
    MemoryChunkPtr Allocate(size_t alignedSize);
private:
    MemoryChunkPtr GetStackChunk(size_t alignedSize) const;
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    MemoryChunkPtr AcquireHeapChunk(size_t alignedSize);
    static MemoryChunkPtr GetAcquiredExactSize(size_t size, const HeapChunksMap& chunks);
    static MemoryChunkPtr GetAcquired(HeapChunksMap::const_iterator from,
                                      HeapChunksMap::const_iterator to);
#endif
    template<size_t count, size_t size>
    static void FillStackChunks(StackChunksMap& chunks);
    static StackChunksMap CreateStackChunks();
    // impl. of TimerHandle::Listener
    void OnTimer(TimerHandle* timer) final;
private:
    static inline constexpr size_t _maxStackChunkSize = 4096U;
    // key is allocated size
    const StackChunksMap _stackChunks;
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    const std::unique_ptr<TimerHandle> _garbageCollectorTimer;
    ProtectedObj<HeapChunksMap> _heapChunks;
#endif
};

PoolAllocator::PoolAllocator()
    : _impl(std::make_unique<AllocatorImpl>())
{
}

PoolAllocator::~PoolAllocator()
{
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
    if (_chunk->IsAcquired() && size <= _chunk->GetSize()) {
        _size = size;
        return true;
    }
    return false;
}

PoolAllocator::AllocatorImpl::AllocatorImpl()
    : _stackChunks(CreateStackChunks())
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    , _garbageCollectorTimer(std::make_unique<TimerHandle>(this))
#endif
{
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    const auto interval = POOL_MEMORY_ALLOCATOR_HEAP_CHUNKS_LIFETIME_SECS * 1000;
    _garbageCollectorTimer->Start(interval, interval);
#endif
}

MemoryChunkPtr PoolAllocator::AllocatorImpl::Allocate(size_t alignedSize)
{
    if (alignedSize) {
        auto chunk = GetStackChunk(alignedSize);
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
        if (!chunk) {
            alignedSize = GetAlignedBufferSize(alignedSize, _maxStackChunkSize);
            chunk = AcquireHeapChunk(alignedSize);
        }
#endif
        return chunk;
    }
    return nullptr;
}

MemoryChunkPtr PoolAllocator::AllocatorImpl::GetStackChunk(size_t alignedSize) const
{
    if (alignedSize && alignedSize <= _maxStackChunkSize) {
        // only exact shooting to cached stack chunks
        if (alignedSize > 1U && alignedSize < _maxStackChunkSize) {
            // find appropriate cache line for avoiding of cache miss
            size_t target = _maxStackChunkSize;
            while (target) {
                if (1U == target / alignedSize) {
                    alignedSize = target;
                    break;
                }
                target = target >> 1;
            }
        }
        const auto itc = _stackChunks.find(alignedSize);
        if (itc != _stackChunks.end()) {
            for (auto it = itc->second.begin(); it != itc->second.end(); ++it) {
                if (it->get()->Acquire()) {
                    return *it;
                }
            }
        }
    }
    return nullptr;
}

#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
MemoryChunkPtr PoolAllocator::AllocatorImpl::AcquireHeapChunk(size_t alignedSize)
{
    LOCK_WRITE_PROTECTED_OBJ(_heapChunks);
    auto chunk = GetAcquiredExactSize(alignedSize, _heapChunks.ConstRef());
    if (!chunk) {
        chunk = GetAcquired(_heapChunks->upper_bound(alignedSize), _heapChunks->end());
        if (!chunk) { // allocate new
            if (auto memory = new (std::nothrow) uint8_t[alignedSize]) {
                auto newChunk = std::make_shared<HeapChunk>(std::unique_ptr<uint8_t[]>(memory), alignedSize);
                if (newChunk->Acquire()) {
                    _heapChunks->insert({alignedSize, newChunk});
                    return newChunk;
                }
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

void PoolAllocator::AllocatorImpl::OnTimer(TimerHandle* timer)
{
#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
    if (timer == _garbageCollectorTimer.get()) {
        const auto maxBufferAgeMs = static_cast<uint32_t>(timer->GetTimeout());
        MS_ASSERT(maxBufferAgeMs, "max buffer age is too small (zero)");
        LOCK_WRITE_PROTECTED_OBJ(_heapChunks);
        for (auto it = _heapChunks->begin(); it != _heapChunks->end();) {
            if (it->second->GetAge() >= maxBufferAgeMs) {
                it = _heapChunks->erase(it);
            }
            else {
                ++it;
            }
        }
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
HeapChunk::HeapChunk(std::unique_ptr<uint8_t[]> memory, size_t size)
    : _memory(std::move(memory))
    , _size(size)
{
}

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
#endif

}
