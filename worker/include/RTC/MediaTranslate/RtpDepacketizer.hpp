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
public:
    virtual ~RtpDepacketizer() = default;
    // [makeDeepCopyOfPayload] it's just a hint
    virtual std::optional<MediaFrame> AddPacket(const RtpPacket* packet,
                                                bool makeDeepCopyOfPayload,
                                                bool* configWasChanged = nullptr) = 0;
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
    MediaFrame CreateMediaFrame() const;
    // factory method - for single-packet frames
    std::optional<MediaFrame> CreateMediaFrame(const RtpPacket* packet,
                                               bool makeDeepCopyOfPayload) const;
    static bool AddPacket(const RtpPacket* packet, MediaFrame* frame,
                          bool makeDeepCopyOfPayload = true);
    static bool AddPacket(const RtpPacket* packet, uint8_t* data, size_t len,
                          MediaFrame* frame, bool makeDeepCopyOfPayload = true);
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
