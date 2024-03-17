#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/Timestamp.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class Buffer;
class BufferAllocator;
class SegmentsBuffer;

class MediaFrame final
{
    class PayloadBufferView;
public:
    MediaFrame() = delete;
    MediaFrame(const RtpCodecMimeType& mimeType, uint32_t clockRate,
               const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    MediaFrame(const MediaFrame& other);
    MediaFrame(MediaFrame&&) = default;
    ~MediaFrame();
    MediaFrame& operator = (const MediaFrame&) = default;
    MediaFrame& operator = (MediaFrame&&) = default;
    void AddPayload(std::shared_ptr<Buffer> payload);
    void AddPayload(uint8_t* data, size_t len, bool makeDeepCopyOfPayload = true);
    std::shared_ptr<const Buffer> GetPayload() const;
    std::shared_ptr<Buffer> TakePayload();
    // common properties
    const RtpCodecMimeType& GetMimeType() const { return _mimeType; }
    bool IsAudio() const { return GetMimeType().IsAudioCodec(); }
    void SetKeyFrame(bool keyFrame);
    bool IsKeyFrame() const { return _keyFrame; }
    uint32_t GetClockRate() const { return GetTimestamp().GetClockRate(); }
    const Timestamp& GetTimestamp() const { return _timestamp; }
    void SetTimestamp(const webrtc::Timestamp& time);
    void SetTimestamp(uint32_t rtpTime);
private:
    RtpCodecMimeType _mimeType;
    std::shared_ptr<SegmentsBuffer> _payload;
    bool _keyFrame = false;
    Timestamp _timestamp;
};

} // namespace RTC	
