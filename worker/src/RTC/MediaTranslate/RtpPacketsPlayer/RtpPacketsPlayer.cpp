#define MS_CLASS "RTC::RtpPacketsPlayer"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMainLoopStream.hpp"
#else
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStream.hpp"
#endif
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayer::RtpPacketsPlayer()
    : RtpPacketsPlayer(std::make_shared<MediaTimer>("RtpPacketsPlayer"))
{
}

RtpPacketsPlayer::RtpPacketsPlayer(const std::shared_ptr<MediaTimer>& timer)
    : _timer(timer)
{
    MS_ASSERT(_timer, "media timer must not be null");
}

RtpPacketsPlayer::~RtpPacketsPlayer()
{
    LOCK_WRITE_PROTECTED_OBJ(_streams);
    _streams->clear();
}

void RtpPacketsPlayer::AddStream(uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
                                 const RtpCodecMimeType& mime,
                                 RtpPacketsPlayerCallback* callback)
{
    if (ssrc && callback) {
        LOCK_WRITE_PROTECTED_OBJ(_streams);
        if (!_streams->count(ssrc)) {
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
            auto stream = RtpPacketsPlayerMainLoopStream::Create(ssrc, clockRate, payloadType, mime, callback);
#else
            auto stream = RtpPacketsPlayerSimpleStream::Create(ssrc, clockRate, payloadType, mime, callback);
#endif
            if (stream) {
                _streams->insert(std::make_pair(ssrc, std::move(stream)));
            }
        }
    }
}

void RtpPacketsPlayer::RemoveStream(uint32_t ssrc)
{
    if (ssrc) {
        LOCK_WRITE_PROTECTED_OBJ(_streams);
        const auto it = _streams->find(ssrc);
        if (it != _streams->end()) {
            _streams->erase(it);
        }
    }
}

bool RtpPacketsPlayer::IsPlaying(uint32_t ssrc) const
{
    if (ssrc) {
        LOCK_READ_PROTECTED_OBJ(_streams);
        const auto it = _streams->find(ssrc);
        if (it != _streams->end()) {
            return it->second->IsPlaying();
        }
    }
    return false;
}

void RtpPacketsPlayer::Play(uint32_t ssrc, uint64_t mediaSourceId,
                            const std::shared_ptr<MemoryBuffer>& media)
{
    if (media && !media->IsEmpty()) {
        LOCK_READ_PROTECTED_OBJ(_streams);
        const auto it = _streams->find(ssrc);
        if (it != _streams->end()) {
            it->second->Play(mediaSourceId, media, _timer);
        }
    }
}

} // namespace RTC
