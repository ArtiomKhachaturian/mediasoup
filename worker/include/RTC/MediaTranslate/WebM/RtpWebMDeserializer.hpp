#pragma once
#include "RTC/MediaTranslate/RtpMediaFrameDeserializer.hpp"

namespace mkvparser {
class IMkvReader;
}

namespace RTC
{

class RtpWebMDeserializer : public RtpMediaFrameDeserializer
{
    class MemoryReader;
    class WebMStream;
public:
    RtpWebMDeserializer(mkvparser::IMkvReader* reader);
    ~RtpWebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    bool Update() final;
    size_t GetTracksCount() const final;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const final;
    std::shared_ptr<const MediaFrame> ReadNextFrame(size_t trackIndex) final;
private:
    std::unique_ptr<WebMStream> _stream;
    bool _ok = true;
};

} // namespace RTC
