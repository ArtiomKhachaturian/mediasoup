#define MS_CLASS "RTC::PoolAllocator"
#include "RTC/MediaTranslate/Buffers/PoolAllocator.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include <array>
#include <atomic>
#include <map>

#define MAX_BLOCKS 32

namespace {

using namespace RTC;

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
    std::array<uint8_t, size> _memory;
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

class HeapChunk : public MemoryChunk
{
public:
    HeapChunk(std::unique_ptr<uint8_t[]> memory, size_t size);
    // impl. of MemoryChunk
    size_t GetSize() const final { return _size; }
    uint8_t* GetData() final { return _memory.get(); }
    const uint8_t* GetData() const final { return _memory.get(); }
private:
    const std::unique_ptr<uint8_t[]> _memory;
    const size_t _size;
};

using MemoryChunkPtr = std::shared_ptr<MemoryChunk>;
using HeapChunkPtr = std::shared_ptr<HeapChunk>;
// key is allocated size
using StackChunksMap = std::multimap<size_t, MemoryChunkPtr>;
using HeapChunksMap = std::multimap<size_t, HeapChunkPtr>;

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


class PoolAllocator::AllocatorImpl
{
public:
    AllocatorImpl();
    std::shared_ptr<Buffer> Allocate(size_t size);
    void PurgeGarbage();
private:
    MemoryChunkPtr AcquireHeapChunk(size_t size);
    template<size_t count, size_t chunkSize>
    static void FillStackChunks(StackChunksMap& chunks);
    static StackChunksMap CreateStackChunks();
    template<class TMap>
    static MemoryChunkPtr GetAcquired(size_t size, const TMap& chunks);
    template<class TIterator>
    static MemoryChunkPtr GetAcquired(TIterator from, TIterator to);
private:
    const StackChunksMap _stackChunks;
    ProtectedObj<HeapChunksMap> _heapChunks;
};

PoolAllocator::PoolAllocator()
    : _impl(std::make_unique<AllocatorImpl>())
{
}

PoolAllocator::~PoolAllocator()
{
}

std::shared_ptr<Buffer> PoolAllocator::Allocate(size_t size)
{
    if (const auto buffer = _impl->Allocate(size)) {
        return buffer;
    }
    return BufferAllocator::Allocate(size);
}

void PoolAllocator::PurgeGarbage()
{
    BufferAllocator::PurgeGarbage();
    _impl->PurgeGarbage();
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
{
}

std::shared_ptr<Buffer> PoolAllocator::AllocatorImpl::Allocate(size_t size)
{
    if (size) {
        auto chunk = GetAcquired(size, _stackChunks);
        if (!chunk) {
            chunk = AcquireHeapChunk(size);
        }
        if (chunk) {
            return std::make_shared<BufferImpl>(std::move(chunk), size);
        }
    }
    return nullptr;
}

void PoolAllocator::AllocatorImpl::PurgeGarbage()
{
    // TODO: 
}

MemoryChunkPtr PoolAllocator::AllocatorImpl::AcquireHeapChunk(size_t size)
{
    LOCK_WRITE_PROTECTED_OBJ(_heapChunks);
    auto chunk = GetAcquired(size, _heapChunks.ConstRef());
    if (!chunk) { // allocate new
        if (auto memory = new (std::nothrow) uint8_t[size]) {
            auto newChunk = std::make_shared<HeapChunk>(std::unique_ptr<uint8_t[]>(memory), size);
            if (newChunk->Acquire()) {
                _heapChunks->insert({size, newChunk});
                return newChunk;
            }
        }
    }
    return chunk;
}

template<size_t count, size_t chunkSize>
void PoolAllocator::AllocatorImpl::FillStackChunks(StackChunksMap& chunks)
{
    for (size_t i = 0U; i < count; ++i) {
        chunks.insert({chunkSize, std::make_shared<StackChunk<chunkSize>>()});
    }
}

StackChunksMap PoolAllocator::AllocatorImpl::CreateStackChunks()
{
    StackChunksMap stackChunks;
    FillStackChunks<MAX_BLOCKS, 1U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 2U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 4U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 8U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 16U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 32U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 64U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 128U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 256U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 512U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 1024U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 2048U>(stackChunks);
    FillStackChunks<MAX_BLOCKS, 4096U>(stackChunks);
    return stackChunks;
}

template<class TMap>
MemoryChunkPtr PoolAllocator::AllocatorImpl::GetAcquired(size_t size, const TMap& chunks)
{
    if (size && !chunks.empty()) {
        const auto exact = chunks.equal_range(size);
        MemoryChunkPtr chunk = GetAcquired(exact.first, exact.second);
        if (!chunk) {
            chunk = GetAcquired(chunks.upper_bound(size), chunks.end());
        }
        return chunk;
    }
    return nullptr;
}

template<class TIterator>
MemoryChunkPtr PoolAllocator::AllocatorImpl::GetAcquired(TIterator from, TIterator to)
{
    for (auto it = from; it != to; ++it) {
        if (it->second->Acquire()) {
            return it->second;
        }
    }
    return nullptr;
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

HeapChunk::HeapChunk(std::unique_ptr<uint8_t[]> memory, size_t size)
    : _memory(std::move(memory))
    , _size(size)
{
}

}
