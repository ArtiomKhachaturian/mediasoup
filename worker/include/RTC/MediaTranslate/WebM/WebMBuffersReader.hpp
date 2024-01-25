#pragma once
#include "RTC/MediaTranslate/WebM/MkvReader.hpp"
#include <array>

namespace RTC
{

class MemoryBuffer;

class WebMBuffersReader : public MkvReader
{
public:
    WebMBuffersReader() = default;
    // impl. of MkvReader
    MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) final;
    // impl. of mkvparser::IMkvReader
    int Read(long long pos, long len, unsigned char* buf) final;
    int Length(long long* total, long long* available) final;
private:
    static inline constexpr size_t _maxBufferSize = 1024UL * 1024UL * 16UL; // 16 Mb
    std::array<uint8_t, _maxBufferSize> _buffer;
    size_t _bufferSize = 0UL;
    uint64_t _buffersCount = 0ULL; // TODO: for debug only, remove in productions
};

} // namespace RTC
