#ifndef MS_RTC_RTP_MEDIA_FRAME_HPP
#define MS_RTC_RTP_MEDIA_FRAME_HPP
#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class RtpMediaConfig;
class RtpAudioConfig;
class RtpVideoConfig;
class RtpPacket;

class RtpMediaFrame
{
public:
    RtpMediaFrame(const RtpCodecMimeType& codecMimeType,
                  std::vector<uint8_t> payload,
                  uint32_t timestamp, uint32_t ssrc,
                  uint16_t sequenceNumber,
                  uint32_t duration = 0U,
                  std::unique_ptr<RtpMediaConfig> mediaConfig = nullptr);
    static std::shared_ptr<RtpMediaFrame> create(const RtpPacket* packet,
                                                 const RtpCodecMimeType& codecMimeType,
                                                 const std::allocator<uint8_t>& payloadAllocator = {},
                                                 uint32_t duration = 0U,
                                                 std::unique_ptr<RtpMediaConfig> mediaConfig = nullptr);
    const RtpCodecMimeType& GetCodecMimeType() const { return _codecMimeType; }
    const std::vector<uint8_t>& GetPayload() const { return _payload; }
    uint32_t GetTimestamp() const { return _timestamp; }
    uint32_t GetSsrc() const { return _ssrc; }
    uint16_t GetSequenceNumber() const { return _sequenceNumber; }
    uint32_t GetDuration() const { return _duration; }
    uint32_t GetAbsSendtime() const { return _absSendtime; }
    void SetAbsSendtime(uint32_t absSendtime) { _absSendtime = absSendtime; }
    const RtpAudioConfig* audioConfig() const;
    const RtpVideoConfig* videoConfig() const;
private:
    static std::vector<uint8_t> CreatePayloadCopy(const RtpPacket* packet,
                                                  const std::allocator<uint8_t>& payloadAllocator = {});
private:
    const RtpCodecMimeType _codecMimeType;
    const std::vector<uint8_t> _payload;
    const uint32_t _timestamp;
    const uint32_t _ssrc;
    const uint16_t _sequenceNumber;
    const uint32_t _duration;
    const std::unique_ptr<RtpMediaConfig> _mediaConfig;
    uint32_t _absSendtime = 0U;
};

class RtpMediaConfig
{
public:
    virtual ~RtpMediaConfig() = default;
};

class RtpAudioConfig : public RtpMediaConfig
{
public:
    RtpAudioConfig(uint8_t channelCount = 1U, uint32_t sampleRate = 48000U);
    uint8_t GetChannelCount() const { return _channelCount; }
    uint32_t GetSampleRate() const { return _sampleRate; }
    void SetChannelCount(uint8_t channelCount) { _channelCount = _channelCount; }
    void SetSampleRate(uint32_t sampleRate) { _sampleRate = sampleRate; }
private:
    uint8_t _channelCount;
    uint32_t _sampleRate;
};

class RtpVideoConfig : public RtpMediaConfig
{
    // TBD
};

} // namespace RTC

#endif
