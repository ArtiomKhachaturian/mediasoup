#define MS_CLASS "RTC::MediaFrame"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/SegmentsMemoryBuffer.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaFrame::MediaFrame(const RtpCodecMimeType& mimeType, uint32_t clockRate)
    : _mimeType(mimeType)
    , _payload(MakeMemoryBuffer<SegmentsMemoryBuffer>())
    , _timestamp(clockRate)
{
    MS_ASSERT(_mimeType.IsMediaCodec(), "invalid media codec");
}

MediaFrame::~MediaFrame()
{
}

void MediaFrame::AddPayload(const std::shared_ptr<MemoryBuffer>& payload)
{
    _payload->Push(payload);
}

std::shared_ptr<const MemoryBuffer> MediaFrame::GetPayload() const
{
    return _payload;
}

void MediaFrame::SetKeyFrame(bool keyFrame)
{
    _keyFrame = keyFrame;
}

void MediaFrame::SetTimestamp(const webrtc::Timestamp& time)
{
    _timestamp.SetTime(time);
}

void MediaFrame::SetTimestamp(uint32_t rtpTime)
{
    _timestamp.SetRtpTime(rtpTime);
}

void MediaFrame::SetMediaConfig(const std::shared_ptr<const MediaFrameConfig>& config)
{
    _config = config;
}

void MediaFrame::SetAudioConfig(const std::shared_ptr<const AudioFrameConfig>& config)
{
    MS_ASSERT(IsAudio(), "set incorrect config for audio track");
    SetMediaConfig(config);
}

std::shared_ptr<const AudioFrameConfig> MediaFrame::GetAudioConfig() const
{
    return std::dynamic_pointer_cast<const AudioFrameConfig>(_config);
}

void MediaFrame::SetVideoConfig(const std::shared_ptr<const VideoFrameConfig>& config)
{
    MS_ASSERT(!IsAudio(), "set incorrect config for video track");
    SetMediaConfig(config);
}

std::shared_ptr<const VideoFrameConfig> MediaFrame::GetVideoConfig() const
{
    return std::dynamic_pointer_cast<const VideoFrameConfig>(_config);
}

MediaFrameConfig::~MediaFrameConfig()
{
}

const std::shared_ptr<const MemoryBuffer>& MediaFrameConfig::GetCodecSpecificData() const
{
    return _codecSpecificData;
}

void MediaFrameConfig::SetCodecSpecificData(const std::shared_ptr<const MemoryBuffer>& data)
{
    _codecSpecificData = data;
}

void MediaFrameConfig::SetCodecSpecificData(const uint8_t* data, size_t len)
{
    SetCodecSpecificData(SimpleMemoryBuffer::Create(data, len));
}

void AudioFrameConfig::SetChannelCount(uint8_t channelCount)
{
    MS_ASSERT(channelCount, "channels count must be greater than zero");
    _channelCount = channelCount;
}

void AudioFrameConfig::SetBitsPerSample(uint8_t bitsPerSample)
{
    MS_ASSERT(bitsPerSample, "bits per sample must be greater than zero");
    MS_ASSERT(0U == bitsPerSample % 8, "bits per sample must be a multiple of 8");
    _bitsPerSample = bitsPerSample;
}

std::string AudioFrameConfig::ToString() const
{
    return std::to_string(GetChannelCount()) + " channels, " +
           std::to_string(GetBitsPerSample()) + " bits";
}

void VideoFrameConfig::SetWidth(int32_t width)
{
    _width = width;
}

void VideoFrameConfig::SetHeight(int32_t height)
{
    _height = height;
}

void VideoFrameConfig::SetFrameRate(double frameRate)
{
    _frameRate = frameRate;
}

std::string VideoFrameConfig::ToString() const
{
    return std::to_string(GetWidth()) + "x" + std::to_string(GetHeight()) +
           " px, " + std::to_string(GetFrameRate()) + " fps";
}

} // namespace RTC
