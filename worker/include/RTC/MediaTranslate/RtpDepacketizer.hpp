#pragma once
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"

namespace RTC
{

class MediaFrame;
class RtpMediaFrameSerializer;
class RtpCodecMimeType;
class RtpPacket;

namespace Codecs {
class PayloadDescriptorHandler;
}

// one instance per each SSRC
class RtpDepacketizer : public BufferAllocations<void>
{
public:
    virtual ~RtpDepacketizer() = default;
    virtual MediaFrame AddPacketInfo(uint32_t rtpTimestamp,
                                     bool keyFrame, bool hasMarker,
                                     const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                     const std::shared_ptr<Buffer>& payload,
                                     bool* configWasChanged = nullptr) = 0;
    MediaFrame AddPacketInfo(const RtpPacket* packet, bool* configWasChanged = nullptr);
    virtual AudioFrameConfig GetAudioConfig() const { return AudioFrameConfig(); }
    virtual VideoFrameConfig GetVideoConfig() const { return VideoFrameConfig(); }
    bool IsAudio() const { return _audio; }
    uint32_t GetSsrc() const { return _ssrc; }
    uint32_t GetClockRate() const { return _clockRate; }
    static std::unique_ptr<RtpDepacketizer> Create(const RtpCodecMimeType& mime,
                                                   uint32_t ssrc, uint32_t clockRate,
                                                   const std::shared_ptr<BufferAllocator>& allocator = nullptr);
protected:
    RtpDepacketizer(bool audio, uint32_t ssrc, uint32_t clockRate,
                    const std::shared_ptr<BufferAllocator>& allocator);
    MediaFrame CreateFrame() const;
    // return true if in/out frame is valid
    bool AddPacketInfoToFrame(uint32_t rtpTimestamp, bool keyFrame,
                              const std::shared_ptr<Buffer>& payload, MediaFrame& frame) const;
    bool AddPacketInfoToFrame(const RtpPacket* packet, MediaFrame& frame) const;
private:
    const bool _audio;
    const uint32_t _ssrc;
    const uint32_t _clockRate;
};

class RtpAudioDepacketizer : public RtpDepacketizer
{
protected:
    RtpAudioDepacketizer(uint32_t ssrc, uint32_t clockRate,
                         const std::shared_ptr<BufferAllocator>& allocator = nullptr);
};

class RtpVideoDepacketizer : public RtpDepacketizer
{
protected:
    RtpVideoDepacketizer(uint32_t ssrc, uint32_t clockRate,
                         const std::shared_ptr<BufferAllocator>& allocator = nullptr);
};

} // namespace RTC
