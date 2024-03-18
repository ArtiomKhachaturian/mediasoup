#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/Buffers/SegmentsBuffer.hpp"

namespace RTC
{

MediaFrame::MediaFrame(uint32_t clockRate, const std::shared_ptr<BufferAllocator>& allocator)
    : _payload(std::make_shared<SegmentsBuffer>(allocator))
    , _timestamp(clockRate)
{
}

MediaFrame::MediaFrame(const MediaFrame& other)
    : _payload(std::make_shared<SegmentsBuffer>(*other._payload))
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
