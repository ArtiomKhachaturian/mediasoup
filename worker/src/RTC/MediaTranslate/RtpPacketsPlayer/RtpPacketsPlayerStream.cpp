#define MS_CLASS "RTC::RtpPacketsPlayerStream"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStreamQueue.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayerStream::RtpPacketsPlayerStream(uint32_t ssrc, uint32_t clockRate,
                                               uint8_t payloadType,
                                               const RtpCodecMimeType& mime,
                                               RtpPacketsPlayerCallback* callback)
    : _ssrc(ssrc)
    , _clockRate(clockRate)
    , _payloadType(payloadType)
    , _mime(mime)
    , _queue(std::make_shared<RtpPacketsPlayerStreamQueue>(callback))
{
    MS_ASSERT(WebMCodecs::IsSupported(_mime), "WebM not available for this MIME %s", _mime.ToString().c_str());
}

RtpPacketsPlayerStream::~RtpPacketsPlayerStream()
{
    _queue->ResetCallback();
}

void RtpPacketsPlayerStream::Play(uint64_t mediaSourceId, const std::shared_ptr<MemoryBuffer>& media,
                                  const std::shared_ptr<MediaTimer>& timer)
{
    if (media && timer) {
        auto deserializer = std::make_unique<WebMDeserializer>();
        auto fragment = std::make_unique<RtpPacketsPlayerMediaFragment>(timer, _queue,
                                                                        std::move(deserializer),
                                                                        _ssrc, _clockRate,
                                                                        _payloadType,
                                                                        media->GetId(),
                                                                        mediaSourceId);
        if (fragment->Parse(_mime, media)) {
            _queue->PushFragment(std::move(fragment));
        }
    }
}

bool RtpPacketsPlayerStream::IsPlaying() const
{
    return _queue->HasFragments();
}

} // namespace RTC
