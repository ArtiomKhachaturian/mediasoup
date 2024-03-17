#define MS_CLASS "RTC::MediaFrame"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/Buffers/SegmentsBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaFrame::MediaFrame(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                       const std::shared_ptr<BufferAllocator>& allocator)
    : _mimeType(mimeType)
    , _payload(std::make_shared<SegmentsBuffer>(allocator))
    , _timestamp(clockRate)
{
    MS_ASSERT(_mimeType.IsMediaCodec(), "invalid media codec");
}

MediaFrame::MediaFrame(const MediaFrame& other)
    : _mimeType(other.GetMimeType())
    , _payload(std::make_shared<SegmentsBuffer>(*other._payload))
    , _timestamp(other._timestamp)
{
}

MediaFrame::~MediaFrame()
{
}

void MediaFrame::AddPayload(std::shared_ptr<Buffer> payload)
{
    _payload->Push(std::move(payload));
}

std::shared_ptr<const Buffer> MediaFrame::GetPayload() const
{
    return _payload;
}

std::shared_ptr<Buffer> MediaFrame::TakePayload()
{
    return _payload->Take();
}

void MediaFrame::SetKeyFrame(bool keyFrame)
{
    _keyFrame = keyFrame;
}

void MediaFrame::SetTimestamp(const webrtc::Timestamp& time)
{
    _timestamp.SetTime(time);
}

void MediaFrame::SetTimestamp(uint32_t rtpTime)
{
    _timestamp.SetRtpTime(rtpTime);
}

} // namespace RTC
