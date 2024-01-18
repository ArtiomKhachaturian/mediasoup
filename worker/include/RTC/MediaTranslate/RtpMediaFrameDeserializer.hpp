#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <vector>
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
    // prepare instance for deserialization, allocate of all needed resources
    virtual bool Start() = 0;
    // cleanup all resources before destroy or restart
    virtual void Stop() {}
    // tracks info maybe actual after calling of 'Start'
    virtual size_t GetTracksCount() const = 0; // all tracks, including subtitles
    virtual std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const = 0;
    virtual std::vector<std::shared_ptr<const MediaFrame>> ReadNextFrames(size_t trackIndex) = 0;
    virtual void SetClockRate(size_t /*trackIndex*/, uint32_t /*clockRate*/) {}
    virtual void SetInitialTimestamp(size_t /*trackIndex*/, uint32_t /*initialTimestamp*/) {}
protected:
    RtpMediaFrameDeserializer() = default;
};

} // namespace RTC
