#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/RtpPacket.hpp"

namespace RTC
{

RtpDepacketizerVpx::RtpDepacketizerVpx(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate)
    : RtpDepacketizer(codecMimeType, sampleRate)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::AddPacket(const RtpPacket* packet)
{
    /*if (packet && packet->GetPayload()) {
        std::unique_ptr<RtpVideoConfig> config;
        bool valid = false;
        if (packet->IsKeyFrame()) {
            if (const auto vp8pd = std::dynamic_pointer_cast<const Codecs::VP8::PayloadDescriptor>(packet->GetPayloadDescriptorHandler())) {
                if (vp8pd->width && vp8pd->height) {
                    valid = true;
                    config = std::make_unique<RtpVideoConfig>(vp8pd->width, vp8pd->height, 30);
                }
            }
        }
        else {
            valid = true;
        }
        if (valid) {
            return RtpMediaFrame::create(packet,
                                         GetCodecMimeType(),
                                         static_cast<uint32_t>(140),
                                         std::move(config));
        }
    }*/
    return nullptr;
}

} // namespace RTC
