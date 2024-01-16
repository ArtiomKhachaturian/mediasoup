#pragma once
#include "RTC/MediaTranslate/RtpMediaFrameDeserializer.hpp"

namespace mkvparser {
}

namespace RTC
{

class RtpWebMDeserializer : public RtpMediaFrameDeserializer
{
    class MemoryReader;
    class WebMStream;
public:
    RtpWebMDeserializer();
    ~RtpWebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) final;
    size_t GetTracksCount() const final;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const final;
    std::shared_ptr<const MediaFrame> ReadNextFrame(size_t trackIndex) final;

private:
    bool ParseLatestIncomingBuffer();
    
private:
    const std::unique_ptr<MemoryReader> _reader;
    std::unique_ptr<WebMStream> _stream;
    bool _ok = true;
};

} // namespace RTC
