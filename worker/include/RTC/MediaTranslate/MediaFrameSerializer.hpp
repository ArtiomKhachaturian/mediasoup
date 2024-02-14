#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
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
    uint32_t GetSsrc() const { return _ssrc; }
    const RtpCodecMimeType& GetMimeType() const { return _mime; }
protected:
    MediaFrameSerializer(uint32_t ssrc, const RtpCodecMimeType& mime);
private:
    const uint32_t _ssrc;
    const RtpCodecMimeType _mime;
};

} // namespace RTC
