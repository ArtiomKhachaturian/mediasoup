#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/RtpPacket.hpp"
#include "MemoryBuffer.hpp"

namespace RTC
{

RtpPacket* RtpPacketizerOpus::AddFrame(const std::shared_ptr<const MediaFrame>& frame)
{
    if (frame) {
        if (const auto payload = frame->GetPayload()) {
            if (const auto packet = RtpPacket::Create(payload->GetData(), payload->GetSize())) {
                packet->SetSequenceNumber(GetNextMediaSequenceNumber());
                packet->SetTimestamp(frame->GetTimestamp());
                packet->SetMarker(true);
                return packet;
            }
        }
    }
    return nullptr;
}

} // namespace RTC
