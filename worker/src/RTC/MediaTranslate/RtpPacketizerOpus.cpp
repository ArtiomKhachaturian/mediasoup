#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"

namespace RTC
{

RtpPacketizerOpus::RtpPacketizerOpus()
    : RtpPacketizer(RtpCodecMimeType::Type::AUDIO, RtpCodecMimeType::Subtype::OPUS)
{
}

std::optional<RtpTranslatedPacket> RtpPacketizerOpus::Add(size_t payloadOffset,
                                                          size_t payloadLength,
                                                          const std::weak_ptr<BufferAllocator>& allocator,
                                                          std::shared_ptr<MediaFrame>&& frame)
{
    if (frame) {
        if (auto packet = Create(frame->GetTimestamp(),
                                 frame->TakePayload(),
                                 payloadOffset, payloadLength,
                                 allocator)) {
            packet->SetMarker(_firstFrame);
            _firstFrame = false;
            return packet;
        }
    }
    return std::nullopt;
}

} // namespace RTC
