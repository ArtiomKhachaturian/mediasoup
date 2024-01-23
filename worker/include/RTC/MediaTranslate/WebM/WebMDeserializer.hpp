#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"

namespace RTC
{

class MkvReader;

class WebMDeserializer : public MediaFrameDeserializer
{
    class WebMStream;
    class TrackInfo;
public:
    WebMDeserializer(std::unique_ptr<MkvReader> reader, bool loopback = false);
    ~WebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) final;
    size_t GetTracksCount() const final;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const final;
    std::vector<std::shared_ptr<const MediaFrame>> ReadNextFrames(size_t trackIndex) final;
    void SetClockRate(size_t trackIndex, uint32_t clockRate) final;
    void SetInitialTimestamp(size_t trackIndex, uint32_t initialTimestamp) final;
private:
    const std::unique_ptr<MkvReader> _reader;
    const std::unique_ptr<WebMStream> _stream;
};

} // namespace RTC
