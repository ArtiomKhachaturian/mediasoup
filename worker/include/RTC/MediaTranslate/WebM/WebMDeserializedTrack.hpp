#pragma once
#include "RTC/RtpDictionaries.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializedTrack.hpp"
#include "RTC/MediaTranslate/WebM/MkvEntry.hpp"

namespace RTC
{

class WebMDeserializedTrack : public BufferAllocations<MediaFrameDeserializedTrack>
{
public:
    static std::unique_ptr<WebMDeserializedTrack> Create(const mkvparser::Tracks* tracks,
                                                         unsigned long trackIndex,
                                                         const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    const RtpCodecMimeType& GetMime() const { return _mime; }
    // impl. of MediaFrameDeserializedTrack
    std::optional<MediaFrame> NextFrame(size_t payloadOffset, bool skipPayload,
                                        size_t payloadExtraSize) final;
private:
    WebMDeserializedTrack(const RtpCodecMimeType& mime, const mkvparser::Track* track,
                          const std::shared_ptr<BufferAllocator>& allocator);
    MkvReadResult AdvanceToNextEntry();
    mkvparser::IMkvReader* GetReader() const;
    static std::optional<RtpCodecMimeType> GetMime(const mkvparser::Track* track);
private:
    const RtpCodecMimeType _mime;
    const mkvparser::Track* const _track;
    MkvEntry _currentEntry;
};

} // namespace RTC
