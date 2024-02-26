#include "RTC/MediaTranslate/RtpPacketizer.hpp"

namespace RTC
{

RtpPacketizer::RtpPacketizer(const RtpCodecMimeType& mime)
    : _mime(mime)
{
}


RtpPacketizer::RtpPacketizer(RtpCodecMimeType::Type type,
                             RtpCodecMimeType::Subtype subtype)
    : _mime(type, subtype)
{
}

std::optional<RtpTranslatedPacket> RtpPacketizer::Create(Timestamp timestampOffset,
                                                         std::shared_ptr<Buffer> buffer,
                                                         size_t payloadOffset,
                                                         size_t payloadLength) const
{
    if (buffer) {
        return std::make_optional<RtpTranslatedPacket>(GetType(), std::move(timestampOffset),
                                                       std::move(buffer), payloadOffset,
                                                       payloadLength);
    }
    return std::nullopt;
}

} // namespace RTC
