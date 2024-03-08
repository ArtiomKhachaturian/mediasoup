#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
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
    _pendingMedias->reserve(1U);
}

RtpPacketsPlayerSimpleStream::~RtpPacketsPlayerSimpleStream()
{
    DeactivateMedia();
    LOCK_WRITE_PROTECTED_OBJ(_pendingMedias);
    _pendingMedias->clear();
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
        if (auto fragment = RtpPacketsPlayerMediaFragment::Parse(mediaSourceId,
                                                                 media, timer,
                                                                 GetAllocator())) {
            const auto trackIndex = GetTrackIndex(fragment.get());
            if (trackIndex.has_value()) {
                fragment->Pause(_paused.load());
                LOCK_WRITE_PROTECTED_OBJ(_activeMedia);
                if (!_activeMedia.ConstRef()) {
                    ActivateMedia(trackIndex.value(), std::move(fragment));
                }
                else {
                    const auto mediaId = media->GetId();
                    LOCK_WRITE_PROTECTED_OBJ(_pendingMedias);
                    auto& pendingMedias = _pendingMedias.Ref();
                    pendingMedias[mediaSourceId][mediaId] = std::make_pair(std::move(fragment), trackIndex.value());
                }
            }
        }
    }
}

void RtpPacketsPlayerSimpleStream::Stop(uint64_t mediaSourceId, uint64_t mediaId)
{
    if (DeactivateMedia(mediaId, mediaSourceId)) {
        ActivateNextPendingMedia();
    }
}

bool RtpPacketsPlayerSimpleStream::IsPlaying() const
{
    LOCK_READ_PROTECTED_OBJ(_activeMedia);
    return nullptr != _activeMedia.ConstRef();
}

void RtpPacketsPlayerSimpleStream::Pause(bool pause)
{
    if (pause != _paused.exchange(pause)) {
        {
            LOCK_READ_PROTECTED_OBJ(_activeMedia);
            if (const auto& activeMedia = _activeMedia.ConstRef()) {
                activeMedia->Pause(pause);
            }
        }
        LOCK_READ_PROTECTED_OBJ(_pendingMedias);
        for (auto it = _pendingMedias->begin(); it != _pendingMedias->end(); ++it) {
            for (auto itm = it->second.begin(); itm != it->second.end(); ++itm) {
                itm->second.first->Pause(pause);
            }
        }
    }
}

void RtpPacketsPlayerSimpleStream::ActivateMedia(size_t trackIndex,
                                                 std::unique_ptr<RtpPacketsPlayerMediaFragment> fragment)
{
    if (fragment) {
        _activeMedia = std::move(fragment);
        _activeMedia->get()->Start(trackIndex, _clockRate, this);
    }
}

void RtpPacketsPlayerSimpleStream::ActivatePendingMedia(PendingMedia pendingMedia)
{
    if (pendingMedia.first) {
        LOCK_WRITE_PROTECTED_OBJ(_activeMedia);
        ActivateMedia(pendingMedia.second, std::move(pendingMedia.first));
    }
}

void RtpPacketsPlayerSimpleStream::ActivateNextPendingMedia()
{
    LOCK_WRITE_PROTECTED_OBJ(_pendingMedias);
    // take next unordered media
    for (auto it = _pendingMedias->begin(); it != _pendingMedias->end(); ++it) {
        bool activated = false;
        for (auto itm = it->second.begin(); itm != it->second.end(); ++itm) {
            ActivatePendingMedia(std::move(itm->second));
            it->second.erase(itm);
            activated = true;
            break;
        }
        if (activated) {
            if (it->second.empty()) {
                _pendingMedias->erase(it);
            }
            break;
        }
    }
}

bool RtpPacketsPlayerSimpleStream::DeactivateMedia(uint64_t mediaId, uint64_t mediaSourceId)
{
    LOCK_WRITE_PROTECTED_OBJ(_activeMedia);
    if (auto& activeMedia = _activeMedia.Ref()) {
        if (activeMedia->GetMediaId() == mediaId && activeMedia->GetMediaSourceId() == mediaSourceId) {
            activeMedia.reset();
            return true;
        }
    }
    return false;
}

void RtpPacketsPlayerSimpleStream::DeactivateMedia()
{
    LOCK_WRITE_PROTECTED_OBJ(_activeMedia);
    _activeMedia->reset();
}

std::optional<size_t> RtpPacketsPlayerSimpleStream::GetTrackIndex(const RtpPacketsPlayerMediaFragment* fragment) const
{
    if (fragment) {
        for (size_t i = 0U; i < fragment->GetTracksCount(); ++i) {
            const auto mime = fragment->GetTrackMimeType(i);
            if (mime.has_value() && mime.value() == _mime) {
                return i;
            }
        }
    }
    return std::nullopt;
}

void RtpPacketsPlayerSimpleStream::OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId)
{
    _callback->OnPlayStarted(mediaId, mediaSourceId, _ssrc);
}

void RtpPacketsPlayerSimpleStream::OnPlay(uint64_t mediaId, uint64_t mediaSourceId,
                                          RtpTranslatedPacket packet)
{
    packet.SetPayloadType(_payloadType);
    packet.SetSsrc(_ssrc);
    _callback->OnPlay(mediaId, mediaSourceId, std::move(packet));
}

void RtpPacketsPlayerSimpleStream::OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId)
{
    _callback->OnPlayFinished(mediaId, mediaSourceId, _ssrc);
    Stop(mediaSourceId, mediaId);
}

} // namespace RTC
