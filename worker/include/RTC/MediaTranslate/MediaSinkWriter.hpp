#pragma once
#include "RTC/MediaTranslate/RtpMediaWriter.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/Timestamp.hpp"
#include "api/units/time_delta.h"
#include <memory>

namespace RTC
{

class MediaFrameWriter;
class RtpDepacketizer;

class MediaSinkWriter : public RtpMediaWriter
{
public:
    MediaSinkWriter(std::unique_ptr<RtpDepacketizer> depacketizer,
                    std::unique_ptr<MediaFrameWriter> frameWriter);
    ~MediaSinkWriter() final;
    // impl. of RtpMediaWriter
    bool WriteRtpMedia(uint32_t ssrc, uint32_t rtpTimestamp,
                       bool keyFrame, bool hasMarker,
                       const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                       const std::shared_ptr<Buffer>& payload) final;
private:
    bool Write(const MediaFrame& mediaFrame);
    MediaFrame CreateFrame(uint32_t ssrc, uint32_t rtpTimestamp,
                           bool keyFrame, bool hasMarker,
                           const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                           const std::shared_ptr<Buffer>& payload);
    const webrtc::TimeDelta& Update(const Timestamp& timestamp);
    bool IsAccepted(const Timestamp& timestamp) const;
private:
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    const std::unique_ptr<MediaFrameWriter> _frameWriter;
    Timestamp _lastTimestamp;
    webrtc::TimeDelta _offset = webrtc::TimeDelta::Zero();
};

} // namespace RTC
