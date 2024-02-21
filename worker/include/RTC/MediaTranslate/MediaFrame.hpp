#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/Timestamp.hpp"
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
    void AddPayload(const std::shared_ptr<MemoryBuffer>& payload);
    std::shared_ptr<const MemoryBuffer> GetPayload() const;
    // common properties
    const RtpCodecMimeType& GetMimeType() const { return _mimeType; }
    bool IsAudio() const { return GetMimeType().IsAudioCodec(); }
    void SetKeyFrame(bool keyFrame);
    bool IsKeyFrame() const { return _keyFrame; }
    uint32_t GetClockRate() const { return GetTimestamp().GetClockRate(); }
    const Timestamp& GetTimestamp() const { return _timestamp; }
    void SetTimestamp(const webrtc::Timestamp& time);
    void SetTimestamp(uint32_t rtpTime);
    void SetMediaConfig(const std::shared_ptr<const MediaFrameConfig>& config);
    // audio configuration
    void SetAudioConfig(const std::shared_ptr<const AudioFrameConfig>& config);
    std::shared_ptr<const AudioFrameConfig> GetAudioConfig() const;
    // video configuration
    void SetVideoConfig(const std::shared_ptr<const VideoFrameConfig>& config);
    std::shared_ptr<const VideoFrameConfig> GetVideoConfig() const;
private:
	const RtpCodecMimeType _mimeType;
    const std::shared_ptr<SegmentsMemoryBuffer> _payload;
    bool _keyFrame = false;
    Timestamp _timestamp;
    std::shared_ptr<const MediaFrameConfig> _config;
};

} // namespace RTC	
