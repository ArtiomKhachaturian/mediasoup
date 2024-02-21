#define MS_CLASS "RTC::RtpMediaFrame"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace {

inline RTC::RtpCodecMimeType::Type GetType(bool audio) {
    return audio ? RTC::RtpCodecMimeType::Type::AUDIO : RTC::RtpCodecMimeType::Type::VIDEO;
}

}

namespace RTC
{

RtpMediaFrame::RtpMediaFrame(bool audio, RtpCodecMimeType::Subtype codecType, uint32_t clockRate)
    : RtpMediaFrame(RtpCodecMimeType(GetType(audio), codecType), clockRate)
{
}

RtpMediaFrame::RtpMediaFrame(const RtpCodecMimeType& mimeType, uint32_t clockRate)
    : MediaFrame(mimeType, clockRate)
{
}

RtpMediaFrame::~RtpMediaFrame()
{
}

bool RtpMediaFrame::AddPacket(const RtpPacket* packet)
{
    return packet && AddPacket(packet, packet->GetPayload(), packet->GetPayloadLength());
}

bool RtpMediaFrame::AddPacket(const RtpPacket* packet, const uint8_t* data, size_t len)
{
    if (packet && AddPayload(SimpleMemoryBuffer::Create(data, len))) {
        if (GetRtpTimestamp() > packet->GetTimestamp()) {
            MS_WARN_TAG(rtp, "time stamp of new packet is less than previous, SSRC = %du", packet->GetSsrc());
        }
        else {
            SetRtpTimestamp(packet->GetTimestamp());
        }
        if (packet->IsKeyFrame()) {
            SetKeyFrame(true);
        }
        return true;
    }
    return false;
}

std::shared_ptr<RtpMediaFrame> RtpMediaFrame::Create(const RtpCodecMimeType& mimeType,
                                                     uint32_t clockRate,
                                                     const RtpPacket* packet)
{
    if (packet) {
        auto frame = std::make_shared<RtpMediaFrame>(mimeType, clockRate);
        if (frame->AddPacket(packet)) {
            return frame;
        }
    }
    return nullptr;
}

std::optional<size_t> RtpMediaFrame::GetPayloadDescriptorSize(const RtpPacket* packet)
{
    if (packet) {
        if (const auto pdh = packet->GetPayloadDescriptorHandler()) {
            return pdh->GetPayloadDescriptorSize();
        }
    }
    return std::nullopt;
}

bool RtpMediaFrame::ParseVp8VideoConfig(const RtpPacket* packet,
                                        const std::shared_ptr<VideoFrameConfig>& applyTo)
{
    if (applyTo) {
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
                        applyTo->SetWidth(hor & 0x3fff);
                        //_videoConfig._widthScale = hor >> 14;
                        applyTo->SetHeight(ver & 0x3fff);
                        //_videoConfig._heightScale = ver >> 14;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool RtpMediaFrame::ParseVp9VideoConfig(const RtpPacket* packet,
                                        const std::shared_ptr<VideoFrameConfig>& applyTo)
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
