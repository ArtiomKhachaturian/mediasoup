#pragma once
#include <mkvparser/mkvreader.h>
#include <array>

namespace RTC
{

class MemoryBuffer;

class WebMBuffersReader : public mkvparser::IMkvReader
{
public:
    WebMBuffersReader() = default;
    bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer);
    // impl. of mkvparser::IMkvReader
    int Read(long long pos, long len, unsigned char* buf) final;
    int Length(long long* total, long long* available) final;
private:
    static inline constexpr size_t _maxBufferSize = 1024UL * 1024UL * 16UL; // 16 Mb
    std::array<uint8_t, _maxBufferSize> _buffer;
    size_t _bufferSize = 0UL;
};

} // namespace RTC
