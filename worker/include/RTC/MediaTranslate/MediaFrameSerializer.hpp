#pragma once
#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
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
class RtpPacket;

namespace Codecs {
class PayloadDescriptorHandler;
}

class MediaFrameSerializer : public BufferAllocations<MediaSource>
{
    class SinkWriter;
    class Queue;
    template<typename K, typename V>
    using ProtectedMap = ProtectedObj<std::unordered_map<K, V>, std::mutex>;
public:
    MediaFrameSerializer(const MediaFrameSerializer&) = delete;
    MediaFrameSerializer(MediaFrameSerializer&&) = delete;
    ~MediaFrameSerializer() override;
    void Write(const RtpPacket* packet);
    void SetPaused(bool paused) { _paused = paused; }
    bool IsReadyToWrite() const { return !IsPaused() && HasSinks(); }
    const RtpCodecMimeType& GetMime() const { return _mime; }
    uint32_t GetClockRate() const { return _clockRate; }
    // impl. of MediaSource
    bool IsPaused() const final { return _paused.load(); }
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final;
    virtual std::string_view GetFileExtension() const;
protected:
    MediaFrameSerializer(const RtpCodecMimeType& mime, uint32_t clockRate,
                         const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    virtual std::unique_ptr<MediaFrameWriter> CreateWriter(uint64_t senderId,
                                                           MediaSink* sink) = 0;
private:
    std::unique_ptr<SinkWriter> CreateSinkWriter(MediaSink* sink);
    void WriteToSinks(uint32_t ssrc, uint32_t rtpTimestamp,
                      bool keyFrame, bool hasMarker,
                      const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                      const std::shared_ptr<Buffer>& payload) const;
    std::string GetMimeText() const { return GetMime().ToString(); }
    static Queue& GetQueue();
private:
    const RtpCodecMimeType _mime;
    const uint32_t _clockRate;
    ProtectedMap<MediaSink*, std::unique_ptr<SinkWriter>> _writers;
    std::atomic_bool _paused = false;
};

} // namespace RTC
