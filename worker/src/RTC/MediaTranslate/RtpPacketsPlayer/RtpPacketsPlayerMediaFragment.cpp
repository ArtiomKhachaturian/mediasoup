#define MS_CLASS "RTC::RtpPacketsPlayerMediaFragment"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragmentQueue.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayerMediaFragment::RtpPacketsPlayerMediaFragment(std::shared_ptr<RtpPacketsPlayerMediaFragmentQueue> queue)
    : _queue(queue)
{
}

RtpPacketsPlayerMediaFragment::~RtpPacketsPlayerMediaFragment()
{
    _queue->Stop();
}

void RtpPacketsPlayerMediaFragment::Start(size_t trackIndex, uint32_t clockRate)
{
    _queue->Start(trackIndex, clockRate);
}

uint64_t RtpPacketsPlayerMediaFragment::GetMediaId() const
{
    return _queue->GetMediaId();
}

uint64_t RtpPacketsPlayerMediaFragment::GetMediaSourceId() const
{
    return _queue->GetMediaSourceId();
}

size_t RtpPacketsPlayerMediaFragment::GetTracksCount() const
{
    return _queue->GetTracksCount();
}

std::optional<RtpCodecMimeType> RtpPacketsPlayerMediaFragment::GetTrackMimeType(size_t trackIndex) const
{
    return _queue->GetTrackType(trackIndex);
}

std::unique_ptr<RtpPacketsPlayerMediaFragment> RtpPacketsPlayerMediaFragment::
    Parse(uint64_t mediaId, uint64_t mediaSourceId,
          const std::shared_ptr<Buffer>& buffer,
          const std::shared_ptr<MediaTimer> playerTimer,
          RtpPacketsPlayerStreamCallback* callback,
          const std::shared_ptr<BufferAllocator>& allocator)
{
    std::unique_ptr<RtpPacketsPlayerMediaFragment> fragment;
    if (buffer && callback && playerTimer) {
        auto deserializer = std::make_unique<WebMDeserializer>(allocator);
        const auto result = deserializer->Add(buffer);
        if (MaybeOk(result)) {
            if (deserializer->GetTracksCount()) {
                auto queue = RtpPacketsPlayerMediaFragmentQueue::Create(mediaId,
                                                                        mediaSourceId,
                                                                        playerTimer,
                                                                        std::move(deserializer),
                                                                        callback);
                if (queue && playerTimer->Register(queue)) {
                    fragment.reset(new RtpPacketsPlayerMediaFragment(std::move(queue)));
                }
                else {
                    // TODO: add error logs
                }
            }
            else {
                MS_ERROR_STD("deserialized media buffer has no supported media tracks");
            }
        }
        else {
            MS_ERROR_STD("media buffer deserialization was failed: %s", ToString(result));
        }
    }
    return fragment;
}

} // namespace RTC
