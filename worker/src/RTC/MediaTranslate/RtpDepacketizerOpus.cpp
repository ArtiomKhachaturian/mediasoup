#define MS_CLASS "RTC::RtpDepacketizerOpus"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
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
    OpusHeadBuffer() = default;
    OpusHeadBuffer(uint32_t sampleRate);
    OpusHeadBuffer(uint8_t channelCount, uint32_t sampleRate);
    uint8_t GetChannelCount() const { return _head._channelCount; }
    // impl. of MemoryBuffer
    size_t GetSize() const final { return sizeof(_head); }
    uint8_t* GetData() final { return reinterpret_cast<uint8_t*>(&_head); }
    const uint8_t* GetData() const final { return reinterpret_cast<const uint8_t*>(&_head); }
private:
    Codecs::Opus::OpusHead _head;
};

RtpDepacketizerOpus::RtpDepacketizerOpus(const RtpCodecMimeType& mimeType, uint32_t sampleRate)
    : RtpDepacketizer(mimeType)
    , _sampleRate(sampleRate)
    , _opusCodecData(std::make_shared<OpusHeadBuffer>(sampleRate))
{
}

RtpDepacketizerOpus::~RtpDepacketizerOpus()
{
}

std::shared_ptr<const RtpMediaFrame> RtpDepacketizerOpus::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload()) {
        const auto ts = packet->GetTimestamp();
        const auto sn = packet->GetSequenceNumber();
        bool stereo = false;
        Codecs::Opus::ParseTOC(packet->GetPayload()[0], nullptr, nullptr, nullptr, &stereo);
        auto config = std::make_shared<AudioFrameConfig>();
        config->SetChannelCount(stereo ? 2U : 1U);
        config->SetBitsPerSample(16U);
        if (_opusCodecData->GetChannelCount() != config->GetChannelCount()) {
            _opusCodecData = std::make_shared<OpusHeadBuffer>(config->GetChannelCount(),
                                                              _sampleRate);
        }
        config->SetCodecSpecificData(_opusCodecData);
        if (auto frame = RtpMediaFrame::Create(GetMimeType(), packet,
                                               GetPayloadAllocator())) {
            frame->SetAudioConfig(config);
            return frame;
        }
    }
    return nullptr;
}

RtpDepacketizerOpus::OpusHeadBuffer::OpusHeadBuffer(uint32_t sampleRate)
{
    _head._sampleRate = sampleRate;
}

RtpDepacketizerOpus::OpusHeadBuffer::OpusHeadBuffer(uint8_t channelCount, uint32_t sampleRate)
    : _head(channelCount, sampleRate)
{
}

} // namespace RTC
