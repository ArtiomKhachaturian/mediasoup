#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/RtpAudioFrameConfig.hpp"
#include "RTC/MediaTranslate/RtpVideoFrameConfig.hpp"
#include "MemoryBuffer.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class RtpPacket;

class RtpMediaFrame
{
    class RtpAudioFrame;
    class RtpVideoFrame;
public:
    RtpMediaFrame(const RtpCodecMimeType& codecMimeType,
                  const std::shared_ptr<const MemoryBuffer>& payload,
                  bool isKeyFrame, uint32_t timestamp, uint32_t ssrc, uint16_t sequenceNumber,
                  uint32_t sampleRate, uint32_t samplesCount);
    virtual ~RtpMediaFrame() = default;
    static std::shared_ptr<RtpMediaFrame> CreateAudio(const RtpPacket* packet,
                                                      RtpCodecMimeType::Subtype codecType,
                                                      uint32_t sampleRate, uint32_t samplesCount,
                                                      const RtpAudioFrameConfig& audioConfig,
                                                      const std::allocator<uint8_t>& payloadAllocator = {});
    static std::shared_ptr<RtpMediaFrame> CreateAudio(const RtpPacket* packet,
                                                      const std::shared_ptr<const MemoryBuffer>& payload,
                                                      RtpCodecMimeType::Subtype codecType,
                                                      uint32_t sampleRate, uint32_t samplesCount,
                                                      const RtpAudioFrameConfig& audioConfig);
    static std::shared_ptr<RtpMediaFrame> CreateVideo(const RtpPacket* packet,
                                                      RtpCodecMimeType::Subtype codecType,
                                                      uint32_t sampleRate, uint32_t samplesCount,
                                                      const RtpVideoFrameConfig& videoConfig,
                                                      const std::allocator<uint8_t>& payloadAllocator = {});
    static std::shared_ptr<RtpMediaFrame> CreateVideo(const RtpPacket* packet,
                                                      const std::shared_ptr<const MemoryBuffer>& payload,
                                                      RtpCodecMimeType::Subtype codecType,
                                                      uint32_t sampleRate, uint32_t samplesCount,
                                                      const RtpVideoFrameConfig& videoConfig);
    // common
    bool IsAudio() const;
    const RtpCodecMimeType& GetCodecMimeType() const { return _codecMimeType; }
    bool IsKeyFrame() const { return _isKeyFrame; }
    const std::shared_ptr<const MemoryBuffer>& GetPayload() const { return _payload; }
    uint32_t GetTimestamp() const { return _timestamp; }
    uint32_t GetSsrc() const { return _ssrc; }
    uint16_t GetSequenceNumber() const { return _sequenceNumber; }
    uint32_t GetSampleRate() const { return _sampleRate; }
    uint32_t GetSamplesCount() const { return _samplesCount; }
    uint32_t GetAbsSendtime() const { return _absSendtime; }
    void SetAbsSendtime(uint32_t absSendtime) { _absSendtime = absSendtime; }
    // audio configuration
    virtual const RtpAudioFrameConfig* GetAudioConfig() const { return nullptr; }
    // video configuration
    virtual const RtpVideoFrameConfig* GetVideoConfig() const { return nullptr; }
private:
    static std::shared_ptr<const MemoryBuffer> CreatePayload(const RtpPacket* packet,
                                                             const std::allocator<uint8_t>& payloadAllocator);
private:
    const RtpCodecMimeType _codecMimeType;
    const std::shared_ptr<const MemoryBuffer> _payload;
    const bool _isKeyFrame;
    const uint32_t _timestamp;
    const uint32_t _ssrc;
    const uint16_t _sequenceNumber;
    const uint32_t _sampleRate;
    const uint32_t _samplesCount;
    uint32_t _absSendtime = 0U;
};

} // namespace RTC
