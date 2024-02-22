#pragma once
#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "ProtectedObj.hpp"
#include "absl/container/flat_hash_map.h"
#include <memory>
#include <string>

namespace webrtc {
class TimeDelta;
}

namespace RTC
{

class MediaFrame;
class MemoryBuffer;
class MediaFrameWriter;

class MediaFrameSerializer : public MediaSource
{
    class SinkWriter;
public:
    MediaFrameSerializer(const MediaFrameSerializer&) = delete;
    MediaFrameSerializer(MediaFrameSerializer&&) = delete;
    ~MediaFrameSerializer() override;
    bool Push(const std::shared_ptr<const MediaFrame>& mediaFrame);
    bool AddTestSink(MediaSink* sink);
    void RemoveTestSink();
    bool HasTestSink() const;
    const RtpCodecMimeType& GetMimeType() const { return _mime; }
    // impl. of MediaSource
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final;
    virtual std::string_view GetFileExtension() const;
protected:
    MediaFrameSerializer(const RtpCodecMimeType& mime);
    virtual std::unique_ptr<MediaFrameWriter> CreateWriter(MediaSink* sink) = 0;
private:
    std::unique_ptr<SinkWriter> CreateSinkWriter(MediaSink* sink);
    void WriteToTestSink(const std::shared_ptr<const MediaFrame>& mediaFrame) const;
private:
    const RtpCodecMimeType _mime;
    ProtectedObj<absl::flat_hash_map<MediaSink*, std::unique_ptr<SinkWriter>>> _writers;
    ProtectedUniquePtr<SinkWriter> _testWriter;
};

} // namespace RTC
