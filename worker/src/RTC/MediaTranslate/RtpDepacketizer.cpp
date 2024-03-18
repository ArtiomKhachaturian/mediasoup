#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpDepacketizer::RtpDepacketizer(bool audio, uint32_t ssrc, uint32_t clockRate,
                                 const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<void>(allocator)
    , _audio(audio)
    , _ssrc(ssrc)
    , _clockRate(clockRate)
{
}

MediaFrame RtpDepacketizer::AddPacketInfo(const RtpPacket* packet, bool* configWasChanged)
{
    if (packet && packet->GetSsrc() == GetSsrc()) {
        const auto payload = AllocateBuffer(packet->GetPayloadLength(), packet->GetPayload());
        return AddPacketInfo(packet->GetTimestamp(),
                             packet->IsKeyFrame(), packet->HasMarker(),
                             packet->GetPayloadDescriptorHandler(),
                             payload);
    }
    return MediaFrame();
}

std::unique_ptr<RtpDepacketizer> RtpDepacketizer::Create(const RtpCodecMimeType& mime,
                                                         uint32_t ssrc, uint32_t clockRate,
                                                         const std::shared_ptr<BufferAllocator>& allocator)
{
    switch (mime.GetSubtype()) {
        case RtpCodecMimeType::Subtype::MULTIOPUS:
            break; // not yet tested
        case RtpCodecMimeType::Subtype::OPUS:
            return std::make_unique<RtpDepacketizerOpus>(ssrc, clockRate, allocator);
        case RtpCodecMimeType::Subtype::VP8:
            return std::make_unique<RtpDepacketizerVpx>(ssrc, clockRate, true, allocator);
        case RtpCodecMimeType::Subtype::VP9:
            break; // not yet tested
        default:
            break;
    }
    return nullptr;
}

MediaFrame RtpDepacketizer::CreateFrame() const
{
    return MediaFrame(GetClockRate(), GetAllocator());
}

bool RtpDepacketizer::AddPacketInfoToFrame(uint32_t rtpTimestamp, bool keyFrame,
                                           const std::shared_ptr<Buffer>& payload,
                                           MediaFrame& frame) const
{
    if (frame) {
        frame.AddPayload(payload);
        if (frame.GetTimestamp().GetRtpTime() > rtpTimestamp) {
            MS_WARN_TAG(rtp, "time stamp of new packet is less than previous, SSRC = %du", GetSsrc());
        }
        else {
            frame.SetTimestamp(rtpTimestamp);
        }
        if (keyFrame) {
            frame.SetKeyFrame(true);
        }
        return true;
    }
    return false;
}

bool RtpDepacketizer::AddPacketInfoToFrame(const RtpPacket* packet, MediaFrame& frame) const
{
    if (packet && frame) {
        const auto rtpTimestamp = packet->GetTimestamp();
        const auto payload = AllocateBuffer(packet->GetPayloadLength(), packet->GetPayload());
        return AddPacketInfoToFrame(rtpTimestamp, packet->IsKeyFrame(), payload, frame);
    }
    return false;
}

RtpAudioDepacketizer::RtpAudioDepacketizer(uint32_t ssrc, uint32_t clockRate,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    : RtpDepacketizer(true, ssrc, clockRate, allocator)
{
}

RtpVideoDepacketizer::RtpVideoDepacketizer(uint32_t ssrc, uint32_t clockRate,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    : RtpDepacketizer(false, ssrc, clockRate, allocator)
{
}

} // namespace RTC
