#pragma once
#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "ProtectedObj.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace webrtc {
class TimeDelta;
}

namespace RTC
{

class AudioFrameConfig;
class VideoFrameConfig;
class MediaFrame;
class MediaFrameWriter;

class MediaFrameSerializer : public MediaSource
{
    class SinkWriter;
public:
    MediaFrameSerializer(const MediaFrameSerializer&) = delete;
    MediaFrameSerializer(MediaFrameSerializer&&) = delete;
    ~MediaFrameSerializer() override;
    void SetPaused(bool paused) { _paused = paused; }
    bool Write(const MediaFrame& mediaFrame);
    void SetConfig(const AudioFrameConfig& config);
    void SetConfig(const VideoFrameConfig& config);
    bool AddTestSink(MediaSink* sink);
    void RemoveTestSink();
    bool HasTestSink() const;
    bool IsReadyToWrite() const { return !IsPaused() && (HasTestSink() || HasSinks()); }
    const RtpCodecMimeType& GetMimeType() const { return _mime; }
    // impl. of MediaSource
    bool IsPaused() const final { return _paused.load(); }
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final;
    virtual std::string_view GetFileExtension() const;
    virtual bool IsAsyncSerialization() const { return false; }
protected:
    MediaFrameSerializer(const RtpCodecMimeType& mime);
    virtual std::unique_ptr<MediaFrameWriter> CreateWriter(MediaSink* sink) = 0;
private:
    std::unique_ptr<SinkWriter> CreateSinkWriter(MediaSink* sink);
    void WriteToTestSink(const MediaFrame& mediaFrame) const;
    template<class TConfig>
    void SetMediaConfig(const TConfig& config);
private:
    const RtpCodecMimeType _mime;
    ProtectedObj<std::unordered_map<MediaSink*, std::unique_ptr<SinkWriter>>> _writers;
    ProtectedUniquePtr<SinkWriter> _testWriter;
    std::atomic_bool _paused = false;
};

} // namespace RTC
