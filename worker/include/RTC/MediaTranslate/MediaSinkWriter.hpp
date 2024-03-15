#pragma once
#include "RTC/Timestamp.hpp"
#include "api/units/time_delta.h"
#include <memory>
#include <optional>

namespace RTC
{

namespace Codecs {
class PayloadDescriptorHandler;
}

class Buffer;
class MediaFrame;
class MediaFrameWriter;
class RtpDepacketizer;

class MediaSinkWriter
{
public:
    MediaSinkWriter(std::unique_ptr<RtpDepacketizer> depacketizer,
                    std::unique_ptr<MediaFrameWriter> frameWriter);
    ~MediaSinkWriter();
    bool Write(uint32_t ssrc, uint32_t rtpTimestamp,
               bool keyFrame, bool hasMarker,
               const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
               const std::shared_ptr<Buffer>& payload);
private:
    bool Write(const MediaFrame& mediaFrame);
    std::optional<MediaFrame> CreateFrame(uint32_t ssrc, uint32_t rtpTimestamp,
                                          bool keyFrame, bool hasMarker,
                                          const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                          const std::shared_ptr<Buffer>& payload);
    const webrtc::TimeDelta& Update(const Timestamp& timestamp);
    bool IsAccepted(const Timestamp& timestamp) const;
private:
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    const std::unique_ptr<MediaFrameWriter> _frameWriter;
    std::optional<Timestamp> _lastTimestamp;
    webrtc::TimeDelta _offset = webrtc::TimeDelta::Zero();
};

} // namespace RTC
