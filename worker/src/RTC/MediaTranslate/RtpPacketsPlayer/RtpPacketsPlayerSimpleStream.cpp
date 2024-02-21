#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"

namespace RTC
{

RtpPacketsPlayerSimpleStream::RtpPacketsPlayerSimpleStream(const std::shared_ptr<MediaTimer>& timer,
                                                           uint32_t ssrc, uint32_t clockRate,
                                                           uint8_t payloadType,
                                                           const RtpCodecMimeType& mime,
                                                           RtpPacketsPlayerCallback* callback)
    : _timer(timer)
    , _ssrc(ssrc)
    , _clockRate(clockRate)
    , _payloadType(payloadType)
    , _mime(mime)
    , _callback(callback)
{
}

RtpPacketsPlayerSimpleStream::~RtpPacketsPlayerSimpleStream()
{
}

std::unique_ptr<RtpPacketsPlayerStream> RtpPacketsPlayerSimpleStream::
    Create(const std::shared_ptr<MediaTimer>& timer, uint32_t ssrc, uint32_t clockRate,
           uint8_t payloadType, const RtpCodecMimeType& mime, RtpPacketsPlayerCallback* callback)
{
    std::unique_ptr<RtpPacketsPlayerStream> stream;
    if (timer && callback) {
        stream.reset(new RtpPacketsPlayerSimpleStream(timer, ssrc, clockRate, payloadType, mime, callback));
    }
    return stream;
}

void RtpPacketsPlayerSimpleStream::Play(uint64_t mediaSourceId, const std::shared_ptr<MemoryBuffer>& media)
{
    if (media) {
        const uint64_t mediaId = media->GetId();
        LOCK_WRITE_PROTECTED_OBJ(_playingMedias);
        bool exists = false;
        auto its = _playingMedias->find(mediaSourceId);
        if (its != _playingMedias->end()) {
            exists = its->second.count(mediaId) > 0U;
        }
        if (!exists) {
            if (auto fragment = RtpPacketsPlayerMediaFragment::Parse(_mime, media, _timer, _ssrc,
                                                                     _clockRate, _payloadType,
                                                                     mediaId, mediaSourceId, this)) {
                if (its == _playingMedias->end()) {
                    its = _playingMedias->insert(std::make_pair(mediaSourceId, MediasMap())).first;
                }
                its->second[mediaId] = fragment;
                _timer->Singleshot(0U, fragment);
            }
        }
    }
}

bool RtpPacketsPlayerSimpleStream::IsPlaying() const
{
    LOCK_READ_PROTECTED_OBJ(_playingMedias);
    return !_playingMedias->empty();
}

void RtpPacketsPlayerSimpleStream::OnPlayStarted(uint32_t ssrc, uint64_t mediaId,
                                                 uint64_t mediaSourceId)
{
    _callback->OnPlayStarted(ssrc, mediaId, mediaSourceId);
}

void RtpPacketsPlayerSimpleStream::OnPlay(const Timestamp& timestampOffset, RtpPacket* packet,
                                          uint64_t mediaId, uint64_t mediaSourceId)
{
    _callback->OnPlay(timestampOffset, packet, mediaId, mediaSourceId);
}

void RtpPacketsPlayerSimpleStream::OnPlayFinished(uint32_t ssrc, uint64_t mediaId,
                                                  uint64_t mediaSourceId)
{
    {
        LOCK_WRITE_PROTECTED_OBJ(_playingMedias);
        const auto its = _playingMedias->find(mediaSourceId);
        if (its != _playingMedias->end()) {
            const auto itm = its->second.find(mediaId);
            if (itm != its->second.end()) {
                its->second.erase(itm);
                if (its->second.empty()) {
                    _playingMedias->erase(its);
                }
            }
        }
    }
    _callback->OnPlayFinished(ssrc, mediaId, mediaSourceId);
}

} // namespace RTC
