#pragma once
#include "RTC/MediaTranslate/WebM/MkvReader.hpp"
#include "RTC/MediaTranslate/CompoundMemoryBuffer.hpp"

namespace RTC
{

class MemoryBuffer;

class WebMBuffersReader : public MkvReader
{
public:
    WebMBuffersReader();
    // impl. of MkvReader
    MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<MemoryBuffer>& buffer) final;
    // impl. of mkvparser::IMkvReader
    int Read(long long pos, long len, unsigned char* buf) final;
    int Length(long long* total, long long* available) final;
private:
    static inline constexpr uint64_t _maxBufferSize = 1024UL * 1024UL; // 1 Mb
    CompoundMemoryBuffer _buffer;
};

} // namespace RTC
