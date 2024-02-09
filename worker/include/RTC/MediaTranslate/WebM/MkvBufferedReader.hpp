#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include "RTC/MediaTranslate/SegmentsMemoryBuffer.hpp"
#include "RTC/MediaTranslate/WebM/MkvReadResult.hpp"
#include <mkvparser/mkvreader.h>

namespace RTC
{

class MkvBufferedReader : private mkvparser::IMkvReader
{
    using Buffer = SegmentsMemoryBuffer;
public:
    MkvBufferedReader();
    ~MkvBufferedReader() final = default;
    MkvReadResult AddBuffer(const std::shared_ptr<MemoryBuffer>& buffer);
    const mkvparser::Segment* GetSegment() const { return _segment.get(); }
    mkvparser::Segment* GetSegment() { return _segment.get(); }
    const mkvparser::Tracks* GetTracks() const;
    static bool IsLive() { return false; }
private:
    MkvReadResult ParseEBMLHeader();
    MkvReadResult ParseSegment();
    // impl. of mkvparser::IMkvReader
    int Read(long long pos, long len, unsigned char* buf) final;
    int Length(long long* total, long long* available) final;
private:
    static inline constexpr uint64_t _maxBufferSize = 1024UL * 1024UL; // 1 Mb
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    std::unique_ptr<mkvparser::Segment> _segment;
    Buffer _buffer;
};

} // namespace RTC
