#define MS_CLASS "RTC::PoolAllocator"
#include "RTC/Buffers/PoolAllocator.hpp"
#include "RTC/Buffers/PoolStackChunk.hpp"
#include "Logger.hpp"
#include "handles/TimerHandle.hpp"
#include "DepLibUV.hpp"
#include <chrono>

namespace RTC
{

class PoolAllocator::HeapChunk : public PoolMemoryChunk
{
    using Rep = std::chrono::time_point<std::chrono::system_clock>::duration::rep;
public:
    // in milliseconds
    uint64_t GetAge() const;
protected:
    HeapChunk() = default;
    // overrides of MemoryChunk
    void OnAcquired() final;
    void OnReleased() final;
private:
    static Rep GetNow() { return std::chrono::system_clock::now().time_since_epoch().count(); }
private:
    std::atomic<Rep> _lastReleaseTime = 0;
};

class PoolAllocator::RegularHeapChunk : public HeapChunk
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
class PoolAllocator::PseudoHeapChunk : public HeapChunk
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

class PoolAllocator::BufferImpl : public Buffer
{
public:
    BufferImpl(std::shared_ptr<PoolMemoryChunk> chunk, size_t size);
    ~BufferImpl() final;
    // impl. of Buffer
    size_t GetSize() const final { return _size.load(); }
    const uint8_t* GetData() const final;
    uint8_t* GetData() final;
    bool Resize(size_t size) final;
private:
    const std::shared_ptr<PoolMemoryChunk> _chunk;
    std::atomic<size_t> _size;
};

PoolAllocator::PoolAllocator()
    : _stackChunks1byte(FillStackChunks<32U, 1U>())
    , _stackChunks2bytes(FillStackChunks<32U, 2U>())
    , _stackChunks4bytes(FillStackChunks<32U, 4U>())
    , _stackChunks8bytes(FillStackChunks<32U, 8U>())
    , _stackChunks16bytes(FillStackChunks<32U, 16U>())
    , _stackChunks32bytes(FillStackChunks<32U, 32U>())
    , _stackChunks64bytes(FillStackChunks<32U, 64U>())
    , _stackChunks128bytes(FillStackChunks<32U, 128U>())
    , _stackChunks256bytes(FillStackChunks<32U, 256U>())
    , _stackChunks512bytes(FillStackChunks<32U, 512U>())
    , _stackChunks1024bytes(FillStackChunks<64U, 1024U>())
    , _stackChunks2048bytes(FillStackChunks<128U, 2048U>())
    , _stackChunks4096bytes(FillStackChunks<32U, 4096U>())
{
}

PoolAllocator::~PoolAllocator()
{
    PurgeGarbage(0U);
}

bool PoolAllocator::RunGarbageCollector(uint32_t intervalMs)
{
    if (BufferAllocator::RunGarbageCollector(intervalMs)) {
        const MutexLock lock(_heapChunksMtx);
        if (!_garbageCollectorTimer) {
            if (DepLibUV::GetLoop()) {
                TimerHandle::Listener* const listener = this;
                _garbageCollectorTimer = std::make_unique<TimerHandle>(listener);
                _garbageCollectorTimer->Start(intervalMs, intervalMs);
            }
        }
        return nullptr != _garbageCollectorTimer;
    }
    return false;
}

void PoolAllocator::PurgeGarbage(uint32_t maxBufferAgeMs)
{
    BufferAllocator::PurgeGarbage(maxBufferAgeMs);
    //size_t deallocated = 0U, still = 0U;
    {
        const MutexLock lock(_heapChunksMtx);
        if (maxBufferAgeMs) {
            // TODO: change to https://en.cppreference.com/w/cpp/container/multimap/erase_if on C++20
            for (auto it = _heapChunks.begin(); it != _heapChunks.end();) {
                if (it->second->GetAge() >= maxBufferAgeMs) {
                    it = _heapChunks.erase(it);
                    //++deallocated;
                }
                else {
                    ++it;
                    //++still;
                }
            }
        }
        else {
            _heapChunks.clear();
        }
    }
    /*if (deallocated || still) {
        float usage = 100.f;
        if (deallocated) {
            usage = std::floor(100 * (float(still) / float(still + deallocated)));
        }
        MS_DUMP_STD("Purge stats: deallocated chunks %zu, still %zu, usage %d %%",
                     deallocated, still, static_cast<int>(usage));
    }*/
}

void PoolAllocator::StopGarbageCollector()
{
    BufferAllocator::StopGarbageCollector();
    const MutexLock lock(_heapChunksMtx);
    _garbageCollectorTimer.reset();
}

std::shared_ptr<Buffer> PoolAllocator::AllocateAligned(size_t size, size_t alignedSize)
{
    if (size) {
        auto chunk = GetStackChunk(alignedSize);
        if (!chunk) {
            chunk = AcquireHeapChunk(alignedSize);
        }
        if (chunk) {
            return std::make_shared<BufferImpl>(std::move(chunk), size);
        }
    }
    return BufferAllocator::AllocateAligned(size, alignedSize);
}

template<size_t count, size_t size>
PoolAllocator::StackChunks<count> PoolAllocator::FillStackChunks()
{
    StackChunks<count> chunks;
    for (size_t i = 0U; i < count; ++i) {
        chunks[i] = std::make_shared<PoolStackChunk<size>>();
    }
    return chunks;
}

size_t PoolAllocator::GetCachelineSize(size_t alignedSize, size_t cachelineTop)
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

template<class TSerialChunks>
std::shared_ptr<PoolMemoryChunk> PoolAllocator::GetAcquired(const TSerialChunks& chunks)
{
    for (size_t i = 0U, end = std::size(chunks); i < end; ++i) {
        if (chunks[i]->Acquire()) {
            return chunks[i];
        }
    }
    return nullptr;
}

std::shared_ptr<PoolMemoryChunk> PoolAllocator::GetAcquired(size_t size, const HeapChunksMap& chunks)
{
    if (size) {
        const auto exact = chunks.equal_range(size);
        return GetAcquired(exact.first, exact.second);
    }
    return nullptr;
}

std::shared_ptr<PoolMemoryChunk> PoolAllocator::
    GetAcquired(HeapChunksMap::const_iterator from, HeapChunksMap::const_iterator to)
{
    for (auto it = from; it != to; ++it) {
        if (it->second->Acquire()) {
            return it->second;
        }
    }
    return nullptr;
}

std::shared_ptr<PoolAllocator::HeapChunk> PoolAllocator::CreateHeapChunk(size_t alignedSize)
{
    std::shared_ptr<HeapChunk> chunk;
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
                default:
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

std::shared_ptr<PoolMemoryChunk> PoolAllocator::GetStackChunk(size_t alignedSize) const
{
    if (alignedSize && alignedSize <= _maxStackChunkSize) {
        // trying exact shooting to cached chunks
        if (alignedSize > 1U && alignedSize < _maxStackChunkSize) {
            // find appropriate cache line for avoiding of cache miss
            alignedSize = GetCachelineSize(alignedSize, _maxStackChunkSize);
        }
        // switch to next cache lines non-effective from mem usage point of view, only exact match
        switch (alignedSize) {
            case 1U:
                return GetAcquired(_stackChunks1byte);
            case 2U:
                return GetAcquired(_stackChunks2bytes);
            case 4U:
                return GetAcquired(_stackChunks4bytes);
            case 8U:
                return GetAcquired(_stackChunks8bytes);
            case 16U:
                return GetAcquired(_stackChunks16bytes);
            case 32U:
                return GetAcquired(_stackChunks32bytes);
            case 64U:
                return GetAcquired(_stackChunks64bytes);
            case 128U:
                return GetAcquired(_stackChunks128bytes);
            case 256U:
                return GetAcquired(_stackChunks256bytes);
            case 512U:
                return GetAcquired(_stackChunks512bytes);
            case 1024U:
                return GetAcquired(_stackChunks1024bytes);
            case 2048U:
                return GetAcquired(_stackChunks2048bytes);
            case 4096U:
                return GetAcquired(_stackChunks4096bytes);
            default:
                break;
        }
    }
    return nullptr;
}

std::shared_ptr<PoolMemoryChunk> PoolAllocator::AcquireHeapChunk(size_t alignedSize)
{
    std::shared_ptr<PoolMemoryChunk> chunk;
    {
        const MutexLock lock(_heapChunksMtx);
        chunk = GetAcquired(alignedSize, _heapChunks);
        if (!chunk) {
            chunk = GetAcquired(_heapChunks.upper_bound(alignedSize), _heapChunks.end());
        }
        if (!chunk) {
            auto heapChunk = CreateHeapChunk(alignedSize);
            if (heapChunk && heapChunk->Acquire()) {
                _heapChunks.insert({alignedSize, heapChunk});
                if (alignedSize != heapChunk->GetSize()) {
                    _heapChunks.insert({heapChunk->GetSize(), heapChunk});
                }
                chunk = std::move(heapChunk);
            }
        }
    }
    return chunk;
}

void PoolAllocator::OnTimer(TimerHandle* timer)
{
    if (timer == _garbageCollectorTimer.get()) {
        PurgeGarbage(static_cast<uint32_t>(timer->GetTimeout()));
    }
}

bool PoolMemoryChunk::Acquire()
{
    if (!_acquired.exchange(true)) {
        OnAcquired();
        return true;
    }
    return false;
}

void PoolMemoryChunk::Release()
{
    if (_acquired.exchange(false)) {
        OnReleased();
    }
}

uint64_t PoolAllocator::HeapChunk::GetAge() const
{
    if (const auto lastReleaseTime = _lastReleaseTime.load()) {
        return std::chrono::milliseconds(GetNow() - lastReleaseTime).count();
    }
    return 0ULL;
}

void PoolAllocator::HeapChunk::OnAcquired()
{
    PoolMemoryChunk::OnAcquired();
    _lastReleaseTime = 0;
}

void PoolAllocator::HeapChunk::OnReleased()
{
    PoolMemoryChunk::OnReleased();
    _lastReleaseTime = GetNow();
}

PoolAllocator::RegularHeapChunk::RegularHeapChunk(uint8_t* memory, size_t size)
    : _memory(memory)
    , _size(size)
{
}

PoolAllocator::RegularHeapChunk::~RegularHeapChunk()
{
    delete [] _memory;
}

PoolAllocator::BufferImpl::BufferImpl(std::shared_ptr<PoolMemoryChunk> chunk, size_t size)
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
    if (size <= _chunk->GetSize()) {
        _size = size;
        return true;
    }
    return false;
}

} // namespace RTC
