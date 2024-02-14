#define MS_CLASS "RTC::RtpPacketsPlayerStream"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStreamQueue.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/RtpPacketsInfoProvider.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayerStream::RtpPacketsPlayerStream(uint32_t ssrc, const RtpCodecMimeType& mime,
                                               const std::shared_ptr<MediaTimer>& timer,
                                               const RtpPacketsInfoProvider* packetsInfoProvider,
                                               RtpPacketsPlayerCallback* callback)
    : _ssrc(ssrc)
    , _mime(mime)
    , _timer(timer)
    , _packetsInfoProvider(packetsInfoProvider)
    , _queue(std::make_shared<RtpPacketsPlayerStreamQueue>(callback))
{
    MS_ASSERT(WebMCodecs::IsSupported(_mime), "WebM not available for this MIME %s", _mime.ToString().c_str());
}

RtpPacketsPlayerStream::~RtpPacketsPlayerStream()
{
    _queue->ResetCallback();
}

void RtpPacketsPlayerStream::Play(uint64_t mediaId, const std::shared_ptr<MemoryBuffer>& buffer,
                                  const void* userData)
{
    if (buffer) {
        const auto clockRate = _packetsInfoProvider->GetClockRate(_ssrc);
        const auto payloadType = _packetsInfoProvider->GetPayloadType(_ssrc);
        auto deserializer = std::make_unique<WebMDeserializer>();
        auto fragment = std::make_unique<RtpPacketsPlayerMediaFragment>(_timer, _queue,
                                                                        std::move(deserializer),
                                                                        _ssrc, clockRate,
                                                                        payloadType,
                                                                        mediaId, userData);
        if (fragment->Parse(_mime, buffer)) {
            _queue->PushFragment(std::move(fragment));
        }
    }
}

bool RtpPacketsPlayerStream::IsPlaying() const
{
    return _queue->HasFragments();
}

} // namespace RTC
