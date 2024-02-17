#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStreamQueue.hpp"

namespace RTC
{

RtpPacketsPlayerSimpleStream::
    RtpPacketsPlayerSimpleStream(std::shared_ptr<RtpPacketsPlayerSimpleStreamQueue> queue)
    : _queue(std::move(queue))
{
}

RtpPacketsPlayerSimpleStream::~RtpPacketsPlayerSimpleStream()
{
    _queue->ResetCallback();
}

std::unique_ptr<RtpPacketsPlayerStream> RtpPacketsPlayerSimpleStream::
    Create(uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
           const RtpCodecMimeType& mime, RtpPacketsPlayerCallback* callback)
{
    std::unique_ptr<RtpPacketsPlayerStream> stream;
    if (callback) {
        if (auto queue = RtpPacketsPlayerSimpleStreamQueue::Create(ssrc, clockRate,
                                                                   payloadType, mime, callback)) {
            stream.reset(new RtpPacketsPlayerSimpleStream(std::move(queue)));
        }
    }
    return stream;
}

void RtpPacketsPlayerSimpleStream::Play(uint64_t mediaSourceId, const std::shared_ptr<MemoryBuffer>& media,
                                        const std::shared_ptr<MediaTimer>& timer)
{
    _queue->Play(mediaSourceId, media, timer);
}

bool RtpPacketsPlayerSimpleStream::IsPlaying() const
{
    return !_queue->IsEmpty();
}

} // namespace RTC
