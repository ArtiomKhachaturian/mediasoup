#pragma once
#include "RTC/MediaTranslate/RtpMediaFrameDeserializer.hpp"

namespace mkvparser {
class Segment;
struct EBMLHeader;
}

namespace RTC
{

class RtpWebMDeserializer : public RtpMediaFrameDeserializer
{
    class MemoryReader;
public:
    RtpWebMDeserializer();
    ~RtpWebMDeserializer() final;
    // impl. of RtpMediaFrameDeserializer
    bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) final;
    size_t GetTracksCount() const final;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackNumber) const final;

private:
    bool ParseLatestIncomingBuffer();
    bool ParseEBMLHeader();
    bool ParseSegment();
    
private:
    const std::unique_ptr<MemoryReader> _reader;
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    mkvparser::Segment* _segment = nullptr;
    bool _ok = true;
};

} // namespace RTC
