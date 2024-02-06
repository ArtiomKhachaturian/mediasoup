#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSourceImpl.hpp"
#include <memory>
#include <string>

namespace RTC
{

class MediaFrame;
class OutputDevice;
class MemoryBuffer;

class MediaFrameSerializer : public MediaSourceImpl
{
public:
    MediaFrameSerializer(const MediaFrameSerializer&) = delete;
    MediaFrameSerializer(MediaFrameSerializer&&) = delete;
    virtual ~MediaFrameSerializer() = default;
    virtual std::string_view GetFileExtension() const;
    virtual bool Push(const std::shared_ptr<const MediaFrame>& mediaFrame) = 0;
    uint32_t GetSsrc() const { return _ssrc; }
    uint32_t GetClockRate() const { return _clockRate; }
    const RtpCodecMimeType& GetMimeType() const { return _mime; }
protected:
    MediaFrameSerializer(uint32_t ssrc, uint32_t clockRate, const RtpCodecMimeType& mime);
private:
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const RtpCodecMimeType _mime;
};

} // namespace RTC
