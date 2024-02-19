#define MS_CLASS "RTC::RtpDepacketizerOpus"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/Codecs/Tools.hpp"
#include "RTC/RtpPacket.hpp"
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

RtpDepacketizerOpus::RtpDepacketizerOpus(const RtpCodecMimeType& mimeType, uint32_t clockRate)
    : RtpDepacketizer(mimeType, clockRate)
    , _opusCodecData(std::make_shared<OpusHeadBuffer>(clockRate))
{
    EnsureStereoAudioConfig(true);
}

RtpDepacketizerOpus::~RtpDepacketizerOpus()
{
}

std::shared_ptr<const MediaFrame> RtpDepacketizerOpus::AddPacket(const RtpPacket* packet)
{
    if (const auto frame = RtpMediaFrame::Create(GetMimeType(), GetClockRate(),
                                                 packet, GetPayloadAllocator())) {
        bool stereo = false;
        Codecs::Opus::ParseTOC(packet->GetPayload(), nullptr, nullptr, nullptr, &stereo);
        frame->SetAudioConfig(EnsureStereoAudioConfig(stereo));
        return frame;
    }
    return nullptr;
}

std::shared_ptr<AudioFrameConfig> RtpDepacketizerOpus::EnsureAudioConfig(uint8_t channelCount)
{
    if (!_audioConfig || _audioConfig->GetChannelCount() != channelCount) {
        _audioConfig = std::make_shared<AudioFrameConfig>();
        _audioConfig->SetChannelCount(channelCount);
        _audioConfig->SetBitsPerSample(_bitsPerSample);
    }
    if (_opusCodecData->GetChannelCount() != channelCount) {
        _opusCodecData = std::make_shared<OpusHeadBuffer>(channelCount, GetClockRate());
    }
    _audioConfig->SetCodecSpecificData(_opusCodecData);
    return _audioConfig;
}

std::shared_ptr<AudioFrameConfig> RtpDepacketizerOpus::EnsureStereoAudioConfig(bool stereo)
{
    return EnsureAudioConfig(stereo ? 2U : 1U);
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
