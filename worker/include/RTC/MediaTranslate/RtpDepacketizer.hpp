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

class RtpDepacketizer : public BufferAllocations<void>
{
    class PayloadBufferView;
public:
    virtual ~RtpDepacketizer() = default;
    virtual std::optional<MediaFrame> CreateFrameFromPacket(uint32_t ssrc, uint32_t rtpTimestamp,
                                                            bool keyFrame,
                                                            const std::shared_ptr<Buffer>& payload,
                                                            bool* configWasChanged = nullptr) = 0;
    std::optional<MediaFrame> CreateFrameFromPacket(const RtpPacket* packet,
                                                    bool* configWasChanged = nullptr);
    virtual AudioFrameConfig GetAudioConfig(const RtpPacket*) const { return AudioFrameConfig(); }
    virtual VideoFrameConfig GetVideoConfig(const RtpPacket*) const { return VideoFrameConfig(); }
    const RtpCodecMimeType& GetMime() const { return _mime; }
    uint32_t GetClockRate() const { return _clockRate; }
    static std::unique_ptr<RtpDepacketizer> Create(const RtpCodecMimeType& mime,
                                                   uint32_t clockRate,
                                                   const std::shared_ptr<BufferAllocator>& allocator = nullptr);
protected:
    RtpDepacketizer(const RtpCodecMimeType& mime, uint32_t clockRate,
                    const std::shared_ptr<BufferAllocator>& allocator);
    MediaFrame CreateFrame() const;
    static bool AddPacketToFrame(uint32_t ssrc, uint32_t rtpTimestamp, bool keyFrame,
                                 const std::shared_ptr<Buffer>& payload, MediaFrame* frame);
    bool AddPacketToFrame(const RtpPacket* packet, MediaFrame* frame) const;
    // null_opt if no descriptor for the packet
    static std::optional<size_t> GetPayloadDescriptorSize(const RtpPacket* packet);
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
