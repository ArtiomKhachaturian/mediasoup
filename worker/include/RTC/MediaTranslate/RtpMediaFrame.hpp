#pragma once

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
    RtpMediaFrame(const RtpCodecMimeType& codecMimeType, bool isKeyFrame,
                  std::vector<uint8_t> payload, uint32_t timestamp, uint32_t ssrc,
                  uint16_t sequenceNumber, uint32_t durationMs,
                  std::unique_ptr<RtpMediaConfig> mediaConfig);
    static std::shared_ptr<RtpMediaFrame> create(const RtpPacket* packet,
                                                 const RtpCodecMimeType& codecMimeType,
                                                 uint32_t durationMs,
                                                 std::unique_ptr<RtpMediaConfig> mediaConfig,
                                                 const std::allocator<uint8_t>& payloadAllocator = {});
    const RtpCodecMimeType& GetCodecMimeType() const { return _codecMimeType; }
    bool IsKeyFrame() const { return _isKeyFrame; }
    const std::vector<uint8_t>& GetPayload() const { return _payload; }
    uint32_t GetTimestamp() const { return _timestamp; }
    uint32_t GetSsrc() const { return _ssrc; }
    uint16_t GetSequenceNumber() const { return _sequenceNumber; }
    uint32_t GetDuration() const { return _durationMs; }
    uint32_t GetAbsSendtime() const { return _absSendtime; }
    void SetAbsSendtime(uint32_t absSendtime) { _absSendtime = absSendtime; }
    const RtpAudioConfig* GetAudioConfig() const;
    const RtpVideoConfig* GetVideoConfig() const;
private:
    static std::vector<uint8_t> CreatePayloadCopy(const RtpPacket* packet,
                                                  const std::allocator<uint8_t>& payloadAllocator = {});
private:
    const RtpCodecMimeType _codecMimeType;
    const bool _isKeyFrame;
    const std::vector<uint8_t> _payload;
    const uint32_t _timestamp;
    const uint32_t _ssrc;
    const uint16_t _sequenceNumber;
    const uint32_t _durationMs;
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
    RtpAudioConfig(uint8_t channelCount = 1U, uint32_t sampleRate = 48000U,
                   uint8_t bitsPerSample = 16U);
    uint8_t GetChannelCount() const { return _channelCount; }
    uint32_t GetSampleRate() const { return _sampleRate; }
    uint8_t GetBitsPerSample() const { return _bitsPerSample; }
    void SetChannelCount(uint8_t channelCount) { _channelCount = channelCount; }
    void SetSampleRate(uint32_t sampleRate) { _sampleRate = sampleRate; }
    void SetBitsPerSample(uint8_t bitsPerSample) { _bitsPerSample = bitsPerSample; }
private:
    uint8_t _channelCount;
    uint32_t _sampleRate;
    uint8_t _bitsPerSample;
};

class RtpVideoConfig : public RtpMediaConfig
{
public:
    RtpVideoConfig(int32_t width, int32_t height, double frameRate);
    int32_t GetWidth() const { return _width; }
    int32_t GetHeight() const { return _height; }
    double GetFrameRate() const { return _frameRate; }
    void SetWidth(int32_t width) { _width = width; }
    void SetHeight(int32_t height) { _height = height; }
    void SetFrameRate(double frameRate) { _frameRate = frameRate; }
private:
    int32_t _width;
    int32_t _height;
    double _frameRate;
};

} // namespace RTC
