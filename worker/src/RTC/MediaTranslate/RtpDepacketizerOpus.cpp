#define MS_CLASS "RTC::RtpDepacketizerOpus"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/RtpPacket.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpDepacketizerOpus::OpusHeadBuffer : public MemoryBuffer
{
public:
    OpusHeadBuffer(uint8_t channelCount, uint32_t sampleRate);
    // impl. of MemoryBuffer
    size_t GetSize() const final { return sizeof(_head); }
    uint8_t* GetData() final { return reinterpret_cast<uint8_t*>(&_head); }
    const uint8_t* GetData() const final { return reinterpret_cast<const uint8_t*>(&_head); }
private:
    Codecs::Opus::OpusHead _head;
};

RtpDepacketizerOpus::RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType,
                                         uint32_t sampleRate)
    : RtpDepacketizer(codecMimeType, sampleRate)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerOpus::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload()) {
        bool stereo = false;
        Codecs::Opus::ParseTOC(packet->GetPayload()[0], nullptr, nullptr, nullptr, &stereo);
        RtpAudioFrameConfig config;
        config._channelCount = stereo ? 2U : 1U;
        config._bitsPerSample = 16U;
        config._codecSpecificData = std::make_unique<OpusHeadBuffer>(config._channelCount,
                                                                     GetSampleRate());
        return RtpMediaFrame::CreateAudio(packet, GetCodecMimeType().GetSubtype(),
                                          GetSampleRate(), std::move(config));
    }
    return nullptr;
}

RtpDepacketizerOpus::OpusHeadBuffer::OpusHeadBuffer(uint8_t channelCount, uint32_t sampleRate)
    : _head(channelCount, sampleRate)
{
}

} // namespace RTC
