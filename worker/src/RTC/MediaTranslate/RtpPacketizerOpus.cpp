#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"

namespace RTC
{

RtpPacketizerOpus::RtpPacketizerOpus(const std::shared_ptr<BufferAllocator>& allocator)
    : RtpPacketizer(RtpCodecMimeType::Type::AUDIO, RtpCodecMimeType::Subtype::OPUS, allocator)
{
}

RtpTranslatedPacket RtpPacketizerOpus::Add(size_t payloadOffset, size_t payloadLength,
                                           MediaFrame&& frame)
{
    auto packet = Create(frame.GetTimestamp(), frame.TakePayload(),
                         payloadOffset, payloadLength);
    if (packet) {
        packet.SetMarker(_firstFrame);
        _firstFrame = false;
    }
    return packet;
}

} // namespace RTC
