#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpVideoFrameConfig.hpp"

namespace RTC
{

class RtpDepacketizerVpx : public RtpDepacketizer
{
public:
    RtpDepacketizerVpx(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate);
    // impl. of RtpDepacketizer
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    static bool ParseVp8VideoConfig(const RtpPacket* packet,
                                    RtpVideoFrameConfig& videoConfig);
    std::shared_ptr<RtpMediaFrame> CreateVp8KeyFrame(const RtpPacket* packet) const;
    std::shared_ptr<RtpMediaFrame> CreateVp9KeyFrame(const RtpPacket* packet) const;
    std::shared_ptr<RtpMediaFrame> CreateInterFrame(const RtpPacket* packet) const;
    std::shared_ptr<RtpMediaFrame> CreateFrame(const RtpPacket* packet,
                                               const RtpVideoFrameConfig& videoConfig) const;
private:
    // https://www.ietf.org/archive/id/draft-ietf-payload-vp8-00.html
    static constexpr inline size_t _vp8VideoConfigOffset = 6U;
};

} // namespace RTC
