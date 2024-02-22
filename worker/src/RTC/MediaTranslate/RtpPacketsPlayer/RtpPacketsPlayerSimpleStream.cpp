#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/Buffers/Buffer.hpp"

namespace RTC
{

RtpPacketsPlayerSimpleStream::RtpPacketsPlayerSimpleStream(const std::shared_ptr<MediaTimer>& timer,
                                                           uint32_t ssrc, uint32_t clockRate,
                                                           uint8_t payloadType,
                                                           const RtpCodecMimeType& mime,
                                                           RtpPacketsPlayerCallback* callback,
                                                           const std::weak_ptr<BufferAllocator>& allocator)
    : BufferAllocations<RtpPacketsPlayerStream>(allocator)
    , _timer(timer)
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
           uint8_t payloadType, const RtpCodecMimeType& mime,
           RtpPacketsPlayerCallback* callback,
           const std::weak_ptr<BufferAllocator>& allocator)
{
    std::unique_ptr<RtpPacketsPlayerStream> stream;
    if (timer && callback) {
        stream.reset(new RtpPacketsPlayerSimpleStream(timer, ssrc, clockRate, payloadType,
                                                      mime, callback, allocator));
    }
    return stream;
}

void RtpPacketsPlayerSimpleStream::Play(uint64_t mediaSourceId, const std::shared_ptr<Buffer>& media)
{
    if (media) {
        const uint64_t mediaId = media->GetId();
        LOCK_WRITE_PROTECTED_OBJ(_playingMedias);
        auto& playingMedias = _playingMedias.Ref();
        if (!IsPlaying(mediaSourceId, mediaId, playingMedias)) {
            if (auto fragment = RtpPacketsPlayerMediaFragment::Parse(_mime, media, _timer,
                                                                     _ssrc, _clockRate,
                                                                     _payloadType, mediaId,
                                                                     mediaSourceId, this,
                                                                     GetAllocator())) {
                playingMedias[mediaSourceId][mediaId] = fragment;
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

bool RtpPacketsPlayerSimpleStream::IsPlaying(uint64_t mediaSourceId, uint64_t mediaId,
                                             const MediaSourcesMap& playingMedias)
{
    if (!playingMedias.empty()) {
        const auto its = playingMedias.find(mediaSourceId);
        if (its != playingMedias.end()) {
            return its->second.count(mediaId) > 0U;
        }
    }
    return false;
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
