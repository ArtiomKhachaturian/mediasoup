#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
#include "RTC/RtpPacket.hpp"

namespace {

using namespace RTC;

std::unique_ptr<Codecs::VP8::PayloadDescriptor> GetVp8PayloadDescriptor(const RtpPacket* packet);

}

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
                return CreateInfaFrame(packet);
            case RtpCodecMimeType::Subtype::VP9:
                if (packet->IsKeyFrame()) {
                    return CreateVp9KeyFrame(packet);
                }
                return CreateInfaFrame(packet);
            default:
                break;
        }
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::CreateVp8KeyFrame(const RtpPacket* packet) const
{
    if (const auto vp8pd = GetVp8PayloadDescriptor(packet)) {
        RtpVideoFrameConfig config;
        config._width = vp8pd->width;
        config._height = vp8pd->height;
        config._frameRate = 30.; // TODO: replace to real value from RTP params
        return CreateFrame(packet, config);
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::CreateVp9KeyFrame(const RtpPacket* packet) const
{
    // TODO: not yet implemented
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::CreateInfaFrame(const RtpPacket* packet) const
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

namespace {

std::unique_ptr<Codecs::VP8::PayloadDescriptor> GetVp8PayloadDescriptor(const RtpPacket* packet) {
    if (packet) {
        const auto data = packet->GetPayload();
        const auto len = packet->GetPayloadLength();
        return std::unique_ptr<Codecs::VP8::PayloadDescriptor>(Codecs::VP8::Parse(data, len));
    }
    return nullptr;
}

}
