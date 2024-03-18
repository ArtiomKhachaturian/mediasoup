#pragma once
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerVpx : public RtpVideoDepacketizer
{
    class ShiftedPayload;
public:
    RtpDepacketizerVpx(uint32_t ssrc, uint32_t clockRate, bool vp8,
                       const std::shared_ptr<BufferAllocator>& allocator);
    ~RtpDepacketizerVpx() final;
    // impl. of RtpDepacketizer
    MediaFrame AddPacketInfo(const RtpPacketInfo& rtpMedia, bool* configWasChanged) final;
    VideoFrameConfig GetVideoConfig() const final { return _config; }
private:
    static bool ParseVp8VideoConfig(const RtpPacketInfo& rtpMedia, VideoFrameConfig& applyTo);
    static bool ParseVp9VideoConfig(const RtpPacketInfo& rtpMedia, VideoFrameConfig& applyTo);
    std::string GetDescription() const;
    MediaFrame MergePacketInfo(const RtpPacketInfo& rtpMedia, bool* configWasChanged);
    bool AddPayload(const RtpPacketInfo& rtpMedia);
    bool ParseVideoConfig(const RtpPacketInfo& rtpMedia, bool* configWasChanged);
    MediaFrame ResetFrame();
    void SetLastTimeStamp(uint32_t lastTimeStamp);
private:
    const bool _vp8;
    uint32_t _lastTimeStamp = 0U;
    MediaFrame _frame;
    VideoFrameConfig _config;
};

} // namespace RTC
