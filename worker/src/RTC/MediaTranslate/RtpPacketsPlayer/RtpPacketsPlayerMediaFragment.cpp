#define MS_CLASS "RTC::RtpPacketsPlayerMediaFragment"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsMediaFragmentPlayer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayerMediaFragment::RtpPacketsPlayerMediaFragment(const std::shared_ptr<MediaTimer>& timer,
                                                             const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                                                             std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                             uint32_t ssrc, uint32_t clockRate,
                                                             uint8_t payloadType,
                                                             uint64_t mediaId, uint64_t mediaSourceId)
    : _player(std::make_shared<RtpPacketsMediaFragmentPlayer>(playerCallbackRef,
                                                              std::move(deserializer),
                                                              ssrc, clockRate,
                                                              payloadType, mediaId, mediaSourceId))
    , _timer(timer)
{
}

RtpPacketsPlayerMediaFragment::~RtpPacketsPlayerMediaFragment()
{
    _timer->UnregisterTimer(_timerId.exchange(0ULL));
}

bool RtpPacketsPlayerMediaFragment::Parse(const RtpCodecMimeType& mime,
                                          const std::shared_ptr<MemoryBuffer>& buffer)
{
    return _player->Parse(mime, buffer);
}

void RtpPacketsPlayerMediaFragment::PlayFrames()
{
    _timerId = _timer->Singleshot(0U, _player);
}

bool RtpPacketsPlayerMediaFragment::IsPlaying() const
{
    return _timer->IsStarted(_timerId.load());
}

uint64_t RtpPacketsPlayerMediaFragment::GetMediaId() const
{
    return _player->GetMediaId();
}

} // namespace RTC
