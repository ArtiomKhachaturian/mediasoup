#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "api/units/timestamp.h"
#include <memory>
#include <vector>

namespace RTC
{

class MediaFrameConfig;
class AudioFrameConfig;
class VideoFrameConfig;
class MemoryBuffer;
class SegmentsMemoryBuffer;

class MediaFrame
{
public:
    MediaFrame(const RtpCodecMimeType& mimeType, uint32_t clockRate);
    virtual ~MediaFrame();
    // add-methods return false if input arguments is incorrect: null or empty payload
    bool AddPayload(std::vector<uint8_t> payload);
    bool AddPayload(const uint8_t* data, size_t len, const std::allocator<uint8_t>& payloadAllocator = {});
    bool AddPayload(const std::shared_ptr<MemoryBuffer>& payload);
    bool IsEmpty() const;
	// expensive operation, in difference from [GetPayloads] returns continuous area of payload data
    std::shared_ptr<const MemoryBuffer> GetPayload() const;
    // common properties
    const RtpCodecMimeType& GetMimeType() const { return _mimeType; }
    bool IsAudio() const { return GetMimeType().IsAudioCodec(); }
    void SetKeyFrame(bool keyFrame);
    bool IsKeyFrame() const { return _keyFrame; }
    uint32_t GetClockRate() const { return _clockRate; }
    webrtc::Timestamp GetTimestamp() const;
    void SetTimestamp(const webrtc::Timestamp& timestamp);
    uint32_t GetRtpTimestamp() const { return _rtpTimestamp; }
    void SetRtpTimestamp(uint32_t rtpTimestamp);
    void SetMediaConfig(const std::shared_ptr<const MediaFrameConfig>& config);
    // audio configuration
    void SetAudioConfig(const std::shared_ptr<const AudioFrameConfig>& config);
    std::shared_ptr<const AudioFrameConfig> GetAudioConfig() const;
    // video configuration
    void SetVideoConfig(const std::shared_ptr<const VideoFrameConfig>& config);
    std::shared_ptr<const VideoFrameConfig> GetVideoConfig() const;
private:
	const RtpCodecMimeType _mimeType;
    const uint32_t _clockRate;
    const std::shared_ptr<SegmentsMemoryBuffer> _payload;
    bool _keyFrame = false;
    uint32_t _rtpTimestamp = 0U;
    std::shared_ptr<const MediaFrameConfig> _config;
};

} // namespace RTC	
