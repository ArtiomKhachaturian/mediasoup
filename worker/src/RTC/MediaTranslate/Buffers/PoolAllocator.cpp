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
    bool Acquire() { return !_acquired.exchange(true); }
    bool Release() { return _acquired.exchange(false); }
protected:
    MemoryChunk() = default;
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
    using ChunksMap = std::multimap<size_t, MemoryChunkPtr>;
public:
    AllocatorImpl();
    std::shared_ptr<Buffer> Allocate(size_t size);
private:
    MemoryChunkPtr AcquireHeapChunk(size_t size);
    template<size_t count, size_t chunkSize>
    static void FillStackChunks(ChunksMap& chunks);
    static ChunksMap CreateStackChunks();
    static MemoryChunkPtr GetAcquired(size_t size, const ChunksMap& chunks);
    static MemoryChunkPtr GetAcquired(ChunksMap::const_iterator from, ChunksMap::const_iterator to);
private:
    const ChunksMap _stackChunks;
    ProtectedObj<ChunksMap> _heapChunks;
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
void PoolAllocator::AllocatorImpl::FillStackChunks(ChunksMap& chunks)
{
    for (size_t i = 0U; i < count; ++i) {
        chunks.insert({chunkSize, std::make_shared<StackChunk<chunkSize>>()});
    }
}

PoolAllocator::AllocatorImpl::ChunksMap PoolAllocator::AllocatorImpl::CreateStackChunks()
{
    ChunksMap stackChunks;
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

MemoryChunkPtr PoolAllocator::AllocatorImpl::GetAcquired(size_t size, const ChunksMap& chunks)
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

MemoryChunkPtr PoolAllocator::AllocatorImpl::GetAcquired(ChunksMap::const_iterator from,
                                                         ChunksMap::const_iterator to)
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

HeapChunk::HeapChunk(std::unique_ptr<uint8_t[]> memory, size_t size)
    : _memory(std::move(memory))
    , _size(size)
{
}

}
