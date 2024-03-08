#pragma once
#include <memory>

namespace webrtc {
class TimeDelta;
}

namespace RTC
{

class MediaFrame;
class AudioFrameConfig;
class VideoFrameConfig;

class MediaFrameWriter
{
public:
    virtual ~MediaFrameWriter() = default;
    virtual bool Write(const MediaFrame& mediaFrame, const webrtc::TimeDelta& offset) = 0;
    virtual void SetConfig(const AudioFrameConfig& config) = 0;
    virtual void SetConfig(const VideoFrameConfig& config) = 0;
    virtual bool IsAsync() const { return false; }
};

} // namespace RTC
