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
    class OffsetEstimator;
    using SinkData = std::pair<std::unique_ptr<MediaFrameWriter>, std::unique_ptr<OffsetEstimator>>;
public:
    MediaFrameSerializer(const MediaFrameSerializer&) = delete;
    MediaFrameSerializer(MediaFrameSerializer&&) = delete;
    ~MediaFrameSerializer() override;
    bool Push(const std::shared_ptr<const MediaFrame>& mediaFrame);
    bool AddTestSink(MediaSink* sink);
    void RemoveTestSink();
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
    void PushToTestSink(const std::shared_ptr<const MediaFrame>& mediaFrame) const;
    static bool Push(const std::shared_ptr<const MediaFrame>& mediaFrame, const SinkData& sink);
private:
    const RtpCodecMimeType _mime;
    ProtectedObj<absl::flat_hash_map<MediaSink*, SinkData>> _sinks;
    ProtectedObj<SinkData> _testSink;
};

} // namespace RTC
