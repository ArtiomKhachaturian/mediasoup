#include "RTC/MediaTranslate/RtpPacketizer.hpp"
#include "RTC/RtpPacketHeader.hpp"

namespace RTC
{

RtpPacketizer::RtpPacketizer(const RtpCodecMimeType& mime,
                             const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<void>(allocator)
    , _mime(mime)
{
}

RtpPacketizer::RtpPacketizer(RtpCodecMimeType::Type type,
                             RtpCodecMimeType::Subtype subtype,
                             const std::shared_ptr<BufferAllocator>& allocator)
    : RtpPacketizer(RtpCodecMimeType(type, subtype), allocator)
{
}

RtpTranslatedPacket RtpPacketizer::Create(Timestamp timestampOffset,
                                          std::shared_ptr<Buffer> buffer,
                                          size_t payloadOffset,
                                          size_t payloadLength) const
{
    if (buffer) {
        return RtpTranslatedPacket(std::move(timestampOffset), std::move(buffer),
                                   payloadOffset, payloadLength, GetAllocator());
    }
    return RtpTranslatedPacket();
}

size_t RtpPacketizer::GetPayloadOffset() const
{
    return sizeof(RtpPacketHeader);
}

size_t RtpPacketizer::GetPayloadExtraSize() const
{
    // minimal size of 1byte header extension with value of RTC::MidMaxLength,
    // see also Producer::MangleRtpPacket & RtpPacket::SetExtensions
    return 16U;
}

} // namespace RTC
