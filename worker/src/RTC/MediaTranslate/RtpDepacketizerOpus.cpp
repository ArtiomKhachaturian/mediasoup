#define MS_CLASS "RTC::RtpDepacketizerOpus"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

RtpDepacketizerOpus::RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType,
                                         uint32_t sampleRate)
    : RtpDepacketizer(codecMimeType, sampleRate)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerOpus::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload()) {
        bool stereo = false;
        Codecs::Opus::FrameSize frameSize;
        Codecs::Opus::ParseTOC(packet->GetPayload()[0], nullptr, nullptr, &frameSize, &stereo);
        RtpAudioFrameConfig config;
        config._channelCount = stereo ? 2U : 1U;
        config._bitsPerSample = 16U;
        return RtpMediaFrame::CreateAudio(packet, GetCodecMimeType().GetSubtype(),
                                          GetSampleRate(), static_cast<uint32_t>(frameSize),
                                          config);
    }
    return nullptr;
}

} // namespace RTC
