#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpDepacketizer::RtpDepacketizer(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                                 const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<void>(allocator)
    , _mimeType(mimeType)
    , _clockRate(clockRate)
{
    MS_ASSERT(_mimeType.IsMediaCodec(), "invalid media codec");
}

std::unique_ptr<RtpDepacketizer> RtpDepacketizer::Create(const RtpCodecMimeType& mimeType,
                                                         uint32_t clockRate,
                                                         const std::shared_ptr<BufferAllocator>& allocator)
{
    switch (mimeType.GetType()) {
        case RtpCodecMimeType::Type::AUDIO:
            switch (mimeType.GetSubtype()) {
                case RtpCodecMimeType::Subtype::MULTIOPUS:
                case RtpCodecMimeType::Subtype::OPUS:
                    return std::make_unique<RtpDepacketizerOpus>(mimeType, clockRate, allocator);
                default:
                    break;
            }
            break;
        case RtpCodecMimeType::Type::VIDEO:
            switch (mimeType.GetSubtype()) {
                case RtpCodecMimeType::Subtype::VP8:
                case RtpCodecMimeType::Subtype::VP9:
                    return std::make_unique<RtpDepacketizerVpx>(mimeType, clockRate, allocator);
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return nullptr;
}

MediaFrame RtpDepacketizer::CreateMediaFrame() const
{
    return MediaFrame(GetMimeType(), GetClockRate(), GetAllocator());
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

bool RtpDepacketizer::ParseVp8VideoConfig(const RtpPacket* packet, VideoFrameConfig& applyTo)
{
    if (const auto pds = GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            const auto offset = pds.value();
            const auto len = packet->GetPayloadLength();
            if (len >= offset + 10U) {
                // Start code for VP8 key frame:
                // Read comon 3 bytes
                //   0 1 2 3 4 5 6 7
                //  +-+-+-+-+-+-+-+-+
                //  |Size0|H| VER |P|
                //  +-+-+-+-+-+-+-+-+
                //  |     Size1     |
                //  +-+-+-+-+-+-+-+-+
                //  |     Size2     |
                //  +-+-+-+-+-+-+-+-+
                // Keyframe header consists of a three-byte sync code
                // followed by the width and height and associated scaling factors
                if (payload[offset + 3U] == 0x9d &&
                    payload[offset + 4U] == 0x01 &&
                    payload[offset + 5U] == 0x2a) {
                    const uint16_t hor = payload[offset + 7U] << 8 | payload[offset + 6U];
                    const uint16_t ver = payload[offset + 9U] << 8 | payload[offset + 8U];
                    applyTo.SetWidth(hor & 0x3fff);
                    //_videoConfig._widthScale = hor >> 14;
                    applyTo.SetHeight(ver & 0x3fff);
                    //_videoConfig._heightScale = ver >> 14;
                    return true;
                }
            }
        }
    }
    return false;
}

bool RtpDepacketizer::ParseVp9VideoConfig(const RtpPacket* packet, VideoFrameConfig& applyTo)
{
    if (const auto pds = GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            //const auto offset = pds.value();
            /*vp9_parser::Vp9HeaderParser parser;
             if (parser.ParseUncompressedHeader(payload, packet->GetPayloadLength())) {
             videoConfig._width = parser.width();
             videoConfig._height = parser.height();
             return true;
             }*/
        }
    }
    return false;
}


} // namespace RTC
