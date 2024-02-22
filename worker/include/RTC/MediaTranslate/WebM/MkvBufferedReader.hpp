#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include "RTC/MediaTranslate/Buffers/SegmentsBuffer.hpp"
#include "RTC/MediaTranslate/WebM/MkvReadResult.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include <mkvparser/mkvreader.h>

namespace RTC
{

class BufferAllocator;

class MkvBufferedReader : private mkvparser::IMkvReader
{
public:
    MkvBufferedReader(const std::weak_ptr<BufferAllocator>& allocator);
    ~MkvBufferedReader() final = default;
    MkvReadResult AddBuffer(const std::shared_ptr<Buffer>& buffer);
    void ClearBuffers();
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
#ifdef MEDIA_TRANSLATIONS_TEST
    static inline constexpr uint64_t _maxBufferSize = 5U * 1024UL * 1024UL; // 5 Mb
#else
    static inline constexpr uint64_t _maxBufferSize = 1024UL * 1024UL; // 1 Mb
#endif
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    std::unique_ptr<mkvparser::Segment> _segment;
    SegmentsBuffer _buffers;
};

} // namespace RTC
