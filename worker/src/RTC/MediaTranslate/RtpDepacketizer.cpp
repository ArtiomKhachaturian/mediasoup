#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpDepacketizer::RtpDepacketizer(const RtpCodecMimeType& mime, uint32_t clockRate,
                                 const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<void>(allocator)
    , _mime(mime)
    , _clockRate(clockRate)
{
    MS_ASSERT(_mime.IsMediaCodec(), "invalid media codec: %s", _mime.ToString().c_str());
}

std::unique_ptr<RtpDepacketizer> RtpDepacketizer::Create(const RtpCodecMimeType& mime,
                                                         uint32_t clockRate,
                                                         const std::shared_ptr<BufferAllocator>& allocator)
{
    switch (mime.GetSubtype()) {
        case RtpCodecMimeType::Subtype::MULTIOPUS:
            break; // not yet tested
        case RtpCodecMimeType::Subtype::OPUS:
            return std::make_unique<RtpDepacketizerOpus>(clockRate, false, allocator);
        case RtpCodecMimeType::Subtype::VP8:
            return std::make_unique<RtpDepacketizerVpx>(clockRate, true, allocator);
        case RtpCodecMimeType::Subtype::VP9:
            break; // not yet tested
        default:
            break;
    }
    return nullptr;
}

MediaFrame RtpDepacketizer::CreateMediaFrame() const
{
    return MediaFrame(GetMime(), GetClockRate(), GetAllocator());
}

std::optional<MediaFrame> RtpDepacketizer::CreateMediaFrame(const RtpPacket* packet,
                                                            bool makeDeepCopyOfPayload) const
{
    if (packet) {
        auto frame = CreateMediaFrame();
        if (AddPacket(packet, &frame, makeDeepCopyOfPayload)) {
            return frame;
        }
    }
    return std::nullopt;
}

bool RtpDepacketizer::AddPacket(const RtpPacket* packet, MediaFrame* frame,
                                bool makeDeepCopyOfPayload)
{
    return packet && frame && AddPacket(packet, packet->GetPayload(),
                                        packet->GetPayloadLength(),
                                        frame, makeDeepCopyOfPayload);
}

bool RtpDepacketizer::AddPacket(const RtpPacket* packet, uint8_t* data, size_t len,
                                MediaFrame* frame, bool makeDeepCopyOfPayload)
{
    if (packet && frame) {
        frame->AddPayload(data, len, makeDeepCopyOfPayload);
        if (frame->GetTimestamp().GetRtpTime() > packet->GetTimestamp()) {
            MS_WARN_TAG(rtp, "time stamp of new packet is less than previous, SSRC = %du", packet->GetSsrc());
        }
        else {
            frame->SetTimestamp(packet->GetTimestamp());
        }
        if (packet->IsKeyFrame()) {
            frame->SetKeyFrame(true);
        }
        return true;
    }
    return false;
}

std::optional<size_t> RtpDepacketizer::GetPayloadDescriptorSize(const RtpPacket* packet)
{
    if (packet) {
        if (const auto pdh = packet->GetPayloadDescriptorHandler()) {
            return pdh->GetPayloadDescriptorSize();
        }
    }
    return std::nullopt;
}

RtpAudioDepacketizer::RtpAudioDepacketizer(RtpCodecMimeType::Subtype type, uint32_t clockRate,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    : RtpDepacketizer(RtpCodecMimeType(RtpCodecMimeType::Type::AUDIO, type), clockRate, allocator)
{
    MS_ASSERT(GetMime().IsAudioCodec(), "invalid audio codec: %s", GetMime().ToString().c_str());
}

RtpVideoDepacketizer::RtpVideoDepacketizer(RtpCodecMimeType::Subtype type, uint32_t clockRate,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    : RtpDepacketizer(RtpCodecMimeType(RtpCodecMimeType::Type::VIDEO, type), clockRate, allocator)
{
    MS_ASSERT(GetMime().IsAudioCodec(), "invalid video codec: %s", GetMime().ToString().c_str());
}

} // namespace RTC
