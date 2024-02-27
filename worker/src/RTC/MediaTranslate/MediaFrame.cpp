#define MS_CLASS "RTC::MediaFrame"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/Buffers/SegmentsBuffer.hpp"
#include "RTC/Buffers/BufferAllocator.hpp"
#include "Logger.hpp"

namespace RTC
{

class MediaFrame::PayloadBufferView : public Buffer
{
public:
    PayloadBufferView(uint8_t* data, size_t len);
    // impl. of Buffer
    size_t GetSize() const final { return _len; }
    uint8_t* GetData() final { return _data; }
    const uint8_t* GetData() const final { return _data; }
private:
    uint8_t* const _data;
    const size_t _len;
};


MediaFrame::MediaFrame(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                       const std::shared_ptr<BufferAllocator>& allocator)
    : _mimeType(mimeType)
    , _payload(std::make_shared<SegmentsBuffer>(allocator))
    , _timestamp(clockRate)
{
    MS_ASSERT(_mimeType.IsMediaCodec(), "invalid media codec");
}

MediaFrame::~MediaFrame()
{
}

void MediaFrame::AddPayload(const std::shared_ptr<Buffer>& payload)
{
    _payload->Push(payload);
}

void MediaFrame::AddPayload(uint8_t* data, size_t len, bool makeDeepCopyOfPayload)
{
    if (data && len) {
        if (makeDeepCopyOfPayload) {
            AddPayload(_payload->AllocateBuffer(len, data));
        }
        else {
            AddPayload(std::make_shared<PayloadBufferView>(data, len));
        }
    }
}

std::shared_ptr<const Buffer> MediaFrame::GetPayload() const
{
    return _payload;
}

std::shared_ptr<Buffer> MediaFrame::TakePayload()
{
    auto oldPayload = _payload;
    _payload = std::make_shared<SegmentsBuffer>(oldPayload->GetAllocator());
    return oldPayload;
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

MediaFrame::PayloadBufferView::PayloadBufferView(uint8_t* data, size_t len)
    : _data(data)
    , _len(len)
{
}

MediaFrameConfig::~MediaFrameConfig()
{
}

const std::shared_ptr<const Buffer>& MediaFrameConfig::GetCodecSpecificData() const
{
    return _codecSpecificData;
}

void MediaFrameConfig::SetCodecSpecificData(const std::shared_ptr<const Buffer>& data)
{
    _codecSpecificData = data;
}

void MediaFrameConfig::SetCodecSpecificData(const uint8_t* data, size_t len,
                                            const std::shared_ptr<BufferAllocator>& allocator)
{
    SetCodecSpecificData(AllocateBuffer(len, data, allocator));
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
