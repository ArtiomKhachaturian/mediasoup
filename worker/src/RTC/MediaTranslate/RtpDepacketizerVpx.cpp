#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
#include "RTC/RtpPacket.hpp"


namespace RTC
{

RtpDepacketizerVpx::RtpDepacketizerVpx(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate)
    : RtpDepacketizer(codecMimeType, sampleRate)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload()) {
        switch (GetCodecMimeType().GetSubtype()) {
            case RtpCodecMimeType::Subtype::VP8:
                if (packet->IsKeyFrame()) {
                    return CreateVp8KeyFrame(packet);
                }
                return CreateInterFrame(packet);
            case RtpCodecMimeType::Subtype::VP9:
                if (packet->IsKeyFrame()) {
                    return CreateVp9KeyFrame(packet);
                }
                return CreateInterFrame(packet);
            default:
                break;
        }
    }
    return nullptr;
}

bool RtpDepacketizerVpx::ParseVp8VideoConfig(const RtpPacket* packet,
                                             RtpVideoFrameConfig& videoConfig)
{
    if (packet && packet->IsKeyFrame()) {
        if (const auto payload = packet->GetPayload()) {
            const auto len = packet->GetPayloadLength();
            if (len >= _vp8VideoConfigOffset + 10U) {
                // Start code for VP8 key frame:
                // Keyframe header consists of a three-byte sync code
                // followed by the width and height and associated scaling factors
                if (payload[_vp8VideoConfigOffset + 3U] == 0x9d &&
                    payload[_vp8VideoConfigOffset + 4U] == 0x01 &&
                    payload[_vp8VideoConfigOffset + 5U] == 0x2a) {
                    const uint16_t hor = payload[_vp8VideoConfigOffset + 7U] << 8 |
                                         payload[_vp8VideoConfigOffset + 6U];
                    const uint16_t ver = payload[_vp8VideoConfigOffset + 9U] << 8 |
                                         payload[_vp8VideoConfigOffset + 8U];
                    videoConfig._width = hor & 0x3fff;
                    videoConfig._widthScale = hor >> 14;
                    videoConfig._height = ver & 0x3fff;
                    videoConfig._heightScale = ver >> 14;
                    return true;
                }
            }
        }
    }
    return false;
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::CreateVp8KeyFrame(const RtpPacket* packet) const
{
    if (packet) {
        RtpVideoFrameConfig config;
        if (ParseVp8VideoConfig(packet, config)) {
            config._frameRate = 30.; // TODO: replace to real value from RTP params
            return CreateFrame(packet, config);
        }
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::CreateVp9KeyFrame(const RtpPacket* packet) const
{
    // TODO: not yet implemented
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::CreateInterFrame(const RtpPacket* packet) const
{
    if (packet) {
        return CreateFrame(packet, {});
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::CreateFrame(const RtpPacket* packet,
                                                               const RtpVideoFrameConfig& videoConfig) const
{
    return RtpMediaFrame::CreateVideo(packet, GetCodecMimeType().GetSubtype(),
                                      GetSampleRate(), videoConfig);
}

} // namespace RTC
