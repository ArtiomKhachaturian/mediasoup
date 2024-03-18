#pragma once
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

class MediaFrame;
class RtpMediaFrameSerializer;
class RtpPacket;

namespace Codecs {
class PayloadDescriptorHandler;
}

class RtpDepacketizer : public BufferAllocations<void>
{
public:
    virtual ~RtpDepacketizer() = default;
    virtual MediaFrame FromRtpPacket(uint32_t ssrc, uint32_t rtpTimestamp,
                                     bool keyFrame, bool hasMarker,
                                     const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                     const std::shared_ptr<Buffer>& payload,
                                     bool* configWasChanged = nullptr) = 0;
    MediaFrame FromRtpPacket(const RtpPacket* packet, bool* configWasChanged = nullptr);
    virtual AudioFrameConfig GetAudioConfig(uint32_t /*ssrc*/) const { return AudioFrameConfig(); }
    virtual VideoFrameConfig GetVideoConfig(uint32_t /*ssrc*/) const { return VideoFrameConfig(); }
    AudioFrameConfig GetAudioConfig(const RtpPacket* packet) const;
    VideoFrameConfig GetVideoConfig(const RtpPacket* packet) const;
    const RtpCodecMimeType& GetMime() const { return _mime; }
    uint32_t GetClockRate() const { return _clockRate; }
    static std::unique_ptr<RtpDepacketizer> Create(const RtpCodecMimeType& mime,
                                                   uint32_t clockRate,
                                                   const std::shared_ptr<BufferAllocator>& allocator = nullptr);
protected:
    RtpDepacketizer(const RtpCodecMimeType& mime, uint32_t clockRate,
                    const std::shared_ptr<BufferAllocator>& allocator);
    MediaFrame CreateFrame() const;
    static void AddPacketToFrame(uint32_t ssrc, uint32_t rtpTimestamp, bool keyFrame,
                                 const std::shared_ptr<Buffer>& payload, MediaFrame& frame);
    void AddPacketToFrame(const RtpPacket* packet, MediaFrame& frame) const;
private:
    const RtpCodecMimeType _mime;
    const uint32_t _clockRate;
};

class RtpAudioDepacketizer : public RtpDepacketizer
{
protected:
    RtpAudioDepacketizer(RtpCodecMimeType::Subtype type, uint32_t clockRate,
                         const std::shared_ptr<BufferAllocator>& allocator);
};

class RtpVideoDepacketizer : public RtpDepacketizer
{
protected:
    RtpVideoDepacketizer(RtpCodecMimeType::Subtype type, uint32_t clockRate,
                         const std::shared_ptr<BufferAllocator>& allocator);
};

} // namespace RTC
