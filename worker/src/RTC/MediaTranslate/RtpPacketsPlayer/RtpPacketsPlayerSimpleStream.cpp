#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/Buffers/Buffer.hpp"

namespace RTC
{

RtpPacketsPlayerSimpleStream::RtpPacketsPlayerSimpleStream(uint32_t ssrc, uint32_t clockRate,
                                                           uint8_t payloadType,
                                                           const RtpCodecMimeType& mime,
                                                           RtpPacketsPlayerCallback* callback,
                                                           const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<RtpPacketsPlayerStream>(allocator)
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
    Create(uint32_t ssrc, uint32_t clockRate,
           uint8_t payloadType, const RtpCodecMimeType& mime,
           RtpPacketsPlayerCallback* callback,
           const std::shared_ptr<BufferAllocator>& allocator)
{
    std::unique_ptr<RtpPacketsPlayerStream> stream;
    if (callback) {
        stream.reset(new RtpPacketsPlayerSimpleStream(ssrc, clockRate, payloadType,
                                                      mime, callback, allocator));
    }
    return stream;
}

void RtpPacketsPlayerSimpleStream::Play(uint64_t mediaSourceId,
                                        const std::shared_ptr<Buffer>& media,
                                        const std::shared_ptr<MediaTimer> timer)
{
    if (media && timer) {
        const uint64_t mediaId = media->GetId();
        LOCK_WRITE_PROTECTED_OBJ(_playingMedias);
        auto& playingMedias = _playingMedias.Ref();
        if (!IsPlaying(mediaSourceId, mediaId, playingMedias)) {
            if (auto fragment = RtpPacketsPlayerMediaFragment::Parse(media, timer, this,
                                                                     GetAllocator())) {
                for (size_t i = 0U; i < fragment->GetTracksCount(); ++i) {
                    const auto mime = fragment->GetTrackMimeType(i);
                    if (mime.has_value() && mime.value() == _mime) {
                        fragment->Start(i, _ssrc, _clockRate, mediaId, mediaSourceId);
                        playingMedias[mediaSourceId][mediaId] = std::move(fragment);
                        break;
                    }
                }
            }
        }
    }
}

void RtpPacketsPlayerSimpleStream::Stop(uint64_t mediaSourceId, uint64_t mediaId)
{
    MediaFragmentsMap fragments;
    {
        LOCK_WRITE_PROTECTED_OBJ(_playingMedias);
        const auto it = _playingMedias->find(mediaSourceId);
        if (it != _playingMedias->end()) {
            if (mediaId) {
                const auto itm = it->second.find(mediaId);
                if (itm != it->second.end()) {
                    fragments[mediaId] = std::move(itm->second);
                    it->second.erase(itm);
                }
            }
            else {
                fragments = std::move(it->second);
            }
            if (it->second.empty()) {
                _playingMedias->erase(it);
            }
        }
    }
    fragments.clear();
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

void RtpPacketsPlayerSimpleStream::OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId,
                                                 uint32_t ssrc)
{
    _callback->OnPlayStarted(mediaId, mediaSourceId, ssrc);
}

void RtpPacketsPlayerSimpleStream::OnPlay(uint64_t mediaId, uint64_t mediaSourceId,
                                          RtpTranslatedPacket packet)
{
    packet.SetPayloadType(_payloadType);
    _callback->OnPlay(mediaId, mediaSourceId, std::move(packet));
}

void RtpPacketsPlayerSimpleStream::OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId,
                                                  uint32_t ssrc)
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
    _callback->OnPlayFinished(mediaId, mediaSourceId, ssrc);
}

} // namespace RTC
