#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include <memory>
#include <string>

namespace RTC
{

class MediaFrame;
class OutputDevice;
class MemoryBuffer;

class MediaFrameSerializer : public MediaSource
{
public:
    MediaFrameSerializer(const MediaFrameSerializer&) = delete;
    MediaFrameSerializer(MediaFrameSerializer&&) = delete;
    virtual ~MediaFrameSerializer() = default;
    virtual std::string_view GetFileExtension() const;
    virtual bool Push(const std::shared_ptr<const MediaFrame>& mediaFrame) = 0;
    virtual bool AddTestSink(MediaSink* sink) { return false; }
    virtual bool RemoveTestSink() { return false; }
    const RtpCodecMimeType& GetMimeType() const { return _mime; }
protected:
    MediaFrameSerializer(const RtpCodecMimeType& mime);
    bool IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const;
    // returns of actual offset from beginning of serialization (starts from zero)
    const webrtc::TimeDelta& UpdateTimeOffset(const webrtc::Timestamp& timestamp);
private:
    const RtpCodecMimeType _mime;
    webrtc::Timestamp _lastTimestamp = webrtc::Timestamp::Zero();
    webrtc::TimeDelta _offset = webrtc::TimeDelta::Zero();
};

} // namespace RTC
