#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/RtpPacket.hpp"

namespace RTC
{

RtpDepacketizerOpus::RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType)
    : RtpDepacketizer(codecMimeType)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerOpus::Assemble(const std::list<const RtpPacket*>& packets) const
{
    if (!packets.empty()) {
        const auto packet = packets.front();
        if (packet && packet->GetPayload()) {
            bool stereo = false;
            Codecs::Opus::FrameSize frameSize;
            Codecs::Opus::ParseTOC(packet->GetPayload()[0], nullptr, nullptr, &frameSize, &stereo);
            auto audioConfig = std::make_unique<RtpAudioConfig>(stereo ? 2U : 1U);
            return RtpMediaFrame::create(packet,
                                         GetCodecMimeType(), static_cast<uint32_t>(frameSize),
                                         std::move(audioConfig), _payloadAllocator);
        }
    }
    return nullptr;
}

} // namespace RTC
