#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpMemoryBufferPacket.hpp"
#include "RTC/Codecs/Opus.hpp"

namespace RTC
{

RtpPacket* RtpPacketizerOpus::AddFrame(const std::shared_ptr<const MediaFrame>& frame)
{
    if (frame) {
        if (const auto payload = frame->GetPayload()) {
            if (const auto packet = RtpMemoryBufferPacket::Create(payload)) {
                packet->SetTimestamp(frame->GetTimestamp());
                packet->SetMarker(_firstFrame);
                Codecs::Opus::ProcessRtpPacket(packet);
                _firstFrame = false;
                return packet;
            }
        }
    }
    return nullptr;
}

} // namespace RTC
