#pragma once
#include "RTC/MediaTranslate/Timestamp.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class Buffer;
class BufferAllocator;
class SegmentsBuffer;

class MediaFrame final
{
public:
    MediaFrame() = default;
    MediaFrame(uint32_t clockRate, const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    MediaFrame(const MediaFrame& other);
    MediaFrame(MediaFrame&&) = default;
    ~MediaFrame();
    MediaFrame& operator = (const MediaFrame&) = default;
    MediaFrame& operator = (MediaFrame&&) = default;
    void AddPayload(std::shared_ptr<Buffer> payload);
    std::shared_ptr<const Buffer> GetPayload() const;
    std::shared_ptr<Buffer> TakePayload();
    // common properties
    constexpr explicit operator bool() const { return IsValid(); }
    bool IsValid() const { return _timestamp.IsValid(); }
    void SetKeyFrame(bool keyFrame);
    bool IsKeyFrame() const { return _keyFrame; }
    uint32_t GetClockRate() const { return GetTimestamp().GetClockRate(); }
    const Timestamp& GetTimestamp() const { return _timestamp; }
    void SetTimestamp(const webrtc::Timestamp& time);
    void SetTimestamp(uint32_t rtpTime);
private:
    std::shared_ptr<SegmentsBuffer> _payload;
    bool _keyFrame = false;
    Timestamp _timestamp;
};

} // namespace RTC	
