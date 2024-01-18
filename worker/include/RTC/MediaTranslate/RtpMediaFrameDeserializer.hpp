#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <optional>

namespace RTC
{

class MediaFrame;

class RtpMediaFrameDeserializer
{
public:
    RtpMediaFrameDeserializer(const RtpMediaFrameDeserializer&) = delete;
    RtpMediaFrameDeserializer(RtpMediaFrameDeserializer&&) = delete;
    virtual ~RtpMediaFrameDeserializer() = default;
    // brings the internal state to the correct form
    virtual bool Update() = 0;
    // tracks info maybe actual after 1st calling of 'Update'
    virtual size_t GetTracksCount() const = 0; // all tracks, including subtitles
    virtual std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const = 0;
    virtual std::shared_ptr<const MediaFrame> ReadNextFrame(size_t trackIndex) = 0;
protected:
    RtpMediaFrameDeserializer() = default;
};

} // namespace RTC
