#include "RTC/Codecs/RtpDepacketizerOpus.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtpMediaFrame.hpp"

namespace RTC
{

RtpDepacketizerOpus::RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType)
    : RtpDepacketizer(codecMimeType)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerOpus::Assemble(const std::list<const RtpPacket*>& packets) const
{
    if (!packets.empty()) {
        if (const auto packet = packets.front()) {
            uint32_t duration = 0;
            std::unique_ptr<RtpMediaConfig> audioConfig;
            if (const auto payload = packet->GetPayload()) {
                bool stereo = false;
                Codecs::Opus::FrameSize frameSize;
                Codecs::Opus::ParseTOC(payload[0], nullptr, nullptr, &frameSize, &stereo);
                audioConfig = std::make_unique<RtpAudioConfig>(stereo ? 2U : 1U);
                duration = static_cast<uint32_t>(frameSize);
            }
            return RtpMediaFrame::create(packet, GetCodecMimeType(),  _payloadAllocator,
                                         duration, std::move(audioConfig));
        }
    }
    return nullptr;
}

} // namespace RTC
