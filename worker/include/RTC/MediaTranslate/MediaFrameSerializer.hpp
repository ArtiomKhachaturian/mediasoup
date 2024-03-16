#pragma once
#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/RtpMediaWriter.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
#include "ProtectedObj.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace RTC
{

class AudioFrameConfig;
class VideoFrameConfig;
class MediaFrame;
class MediaFrameWriter;
class MediaSinkWriter;
class RtpMediaWritersQueue;
class RtpPacket;

class MediaFrameSerializer : public BufferAllocations<MediaSource>,
                             private RtpMediaWriter
{
    using MediaSinkWriterMap = std::unordered_map<MediaSink*, std::unique_ptr<MediaSinkWriter>>;
public:
    MediaFrameSerializer(const MediaFrameSerializer&) = delete;
    MediaFrameSerializer(MediaFrameSerializer&&) = delete;
    ~MediaFrameSerializer() override;
    void Write(const RtpPacket* packet);
    void SetPaused(bool paused) { _paused = paused; }
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
    static RtpMediaWritersQueue& GetQueue();
    std::string GetMimeText() const { return GetMime().ToString(); }
    bool IsReadyToWrite() const { return !IsPaused() && HasSinks(); }
    std::unique_ptr<MediaSinkWriter> CreateSinkWriter(MediaSink* sink);
    // impl. of RtpMediaWriter
    bool WriteRtpMedia(uint32_t ssrc, uint32_t rtpTimestamp, bool keyFrame, bool hasMarker,
                       const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                       const std::shared_ptr<Buffer>& payload) final;
private:
    const RtpCodecMimeType _mime;
    const uint32_t _clockRate;
    ProtectedObj<MediaSinkWriterMap, std::mutex> _writers;
    std::atomic<size_t> _writersCount = 0U; // for quick access without mutex locking
    std::atomic_bool _paused = false;
};

} // namespace RTC
