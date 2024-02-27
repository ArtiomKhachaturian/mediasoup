#include "RTC/MediaTranslate/RtpPacketizer.hpp"
#include "RTC/RtpPacket.hpp"

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
                                                         size_t payloadLength,
                                                         const std::weak_ptr<BufferAllocator>& allocator) const
{
    if (buffer && payloadOffset + payloadLength) {
        return std::make_optional<RtpTranslatedPacket>(GetType(),
                                                       std::move(timestampOffset),
                                                       std::move(buffer),
                                                       payloadOffset,
                                                       payloadLength,
                                                       allocator);
    }
    return std::nullopt;
}

size_t RtpPacketizer::GetPayloadOffset() const
{
    return RtpPacket::HeaderSize;
}

} // namespace RTC
