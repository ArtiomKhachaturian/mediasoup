#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"

namespace RTC
{

RtpPacketizerOpus::RtpPacketizerOpus(const std::shared_ptr<BufferAllocator>& allocator)
    : RtpPacketizer(RtpCodecMimeType::Type::AUDIO, RtpCodecMimeType::Subtype::OPUS, allocator)
{
}

std::optional<RtpTranslatedPacket> RtpPacketizerOpus::Add(size_t payloadOffset,
                                                          size_t payloadLength,
                                                          std::shared_ptr<MediaFrame>&& frame)
{
    if (frame) {
        if (auto packet = Create(frame->GetTimestamp(), frame->TakePayload(),
                                 payloadOffset, payloadLength)) {
            packet->SetMarker(_firstFrame);
            _firstFrame = false;
            return packet;
        }
    }
    return std::nullopt;
}

} // namespace RTC
