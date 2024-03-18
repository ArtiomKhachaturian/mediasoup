#define MS_CLASS "RTC::RtpDepacketizerOpus"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpPacketInfo.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/Codecs/Tools.hpp"
#include "RTC/Buffers/Buffer.hpp"
#include "Logger.hpp"

namespace RTC
{

class RtpDepacketizerOpus::OpusHeadBuffer : public Buffer
{
public:
    OpusHeadBuffer() = default;
    OpusHeadBuffer(uint32_t sampleRate);
    OpusHeadBuffer(uint8_t channelCount, uint32_t sampleRate);
    uint8_t GetChannelCount() const { return _head._channelCount; }
    static uint8_t GetChannelCount(const uint8_t* payload);
    static uint8_t GetChannelCount(const std::shared_ptr<Buffer>& payload);
    // impl. of Buffer
    size_t GetSize() const final { return sizeof(_head); }
    uint8_t* GetData() final { return reinterpret_cast<uint8_t*>(&_head); }
    const uint8_t* GetData() const final { return reinterpret_cast<const uint8_t*>(&_head); }
private:
    Codecs::Opus::OpusHead _head;
};

RtpDepacketizerOpus::RtpDepacketizerOpus(uint32_t ssrc, uint32_t clockRate,
                                         const std::shared_ptr<BufferAllocator>& allocator)
    : RtpAudioDepacketizer(ssrc, clockRate, allocator)
{
    _config.SetBitsPerSample(_bitsPerSample);
}

RtpDepacketizerOpus::~RtpDepacketizerOpus()
{
}

MediaFrame RtpDepacketizerOpus::AddPacketInfo(const RtpPacketInfo& rtpMedia,
                                              bool* configWasChanged)
{
    if (const auto& payload = rtpMedia._payload) {
        auto frame = CreateFrame();
        if (AddPacketInfoToFrame(rtpMedia, frame)) {
            const auto channelsCount = OpusHeadBuffer::GetChannelCount(payload);
            if (!_opusCodecData || channelsCount != _opusCodecData->GetChannelCount()) {
                _opusCodecData = std::make_shared<OpusHeadBuffer>(channelsCount, GetClockRate());
                _config.SetChannelCount(channelsCount);
                _config.SetCodecSpecificData(_opusCodecData);
                if (configWasChanged) {
                    *configWasChanged = true;
                }
            }
            return frame;
        }
    }
    return MediaFrame();
}

RtpDepacketizerOpus::OpusHeadBuffer::OpusHeadBuffer(uint32_t sampleRate)
{
    _head._sampleRate = sampleRate;
}

RtpDepacketizerOpus::OpusHeadBuffer::OpusHeadBuffer(uint8_t channelCount, uint32_t sampleRate)
    : _head(channelCount, sampleRate)
{
}

uint8_t RtpDepacketizerOpus::OpusHeadBuffer::GetChannelCount(const uint8_t* payload)
{
    bool stereo = false;
    Codecs::Opus::ParseTOC(payload, nullptr, nullptr, nullptr, &stereo);
    return stereo ? 2U : 1U;
}

uint8_t RtpDepacketizerOpus::OpusHeadBuffer::GetChannelCount(const std::shared_ptr<Buffer>& payload)
{
    return GetChannelCount(payload->GetData());
}

} // namespace RTC
