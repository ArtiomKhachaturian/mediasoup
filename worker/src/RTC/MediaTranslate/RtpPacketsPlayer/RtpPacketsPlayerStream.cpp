#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStreamQueue.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"

namespace RTC
{

RtpPacketsPlayerStream::RtpPacketsPlayerStream(std::shared_ptr<RtpPacketsPlayerStreamQueue> queue)
    : _queue(std::move(queue))
{
}

RtpPacketsPlayerStream::~RtpPacketsPlayerStream()
{
    _queue->ResetCallback();
}

std::unique_ptr<RtpPacketsPlayerStream> RtpPacketsPlayerStream::Create(uint32_t ssrc,
                                                                       uint32_t clockRate,
                                                                       uint8_t payloadType,
                                                                       const RtpCodecMimeType& mime,
                                                                       RtpPacketsPlayerCallback* callback)
{
    std::unique_ptr<RtpPacketsPlayerStream> stream;
    if (callback) {
        if (auto queue = RtpPacketsPlayerStreamQueue::Create(ssrc, clockRate,
                                                             payloadType, mime, callback)) {
            stream.reset(new RtpPacketsPlayerStream(std::move(queue)));
        }
    }
    return stream;
}

void RtpPacketsPlayerStream::Play(uint64_t mediaSourceId, const std::shared_ptr<MemoryBuffer>& media,
                                  const std::shared_ptr<MediaTimer>& timer)
{
    _queue->Play(mediaSourceId, media, timer);
}

bool RtpPacketsPlayerStream::IsPlaying() const
{
    return !_queue->IsEmpty();
}

uint32_t RtpPacketsPlayerStream::GetSsrc() const
{
    return _queue->GetSsrc();
}

} // namespace RTC
