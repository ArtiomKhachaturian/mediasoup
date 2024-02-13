#define MS_CLASS "RTC::RtpPacketsPlayer"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayer::RtpPacketsPlayer()
    : _timer(std::make_shared<MediaTimer>("RtpPacketsPlayer"))
{
}

RtpPacketsPlayer::~RtpPacketsPlayer()
{
    LOCK_WRITE_PROTECTED_OBJ(_streams);
    _streams->clear();
}

void RtpPacketsPlayer::AddStream(uint32_t ssrc, const RtpCodecMimeType& mime,
                                 RtpPacketsPlayerCallback* callback,
                                 const RtpPacketsInfoProvider* packetsInfoProvider)
{
    if (ssrc && callback && packetsInfoProvider) {
        LOCK_WRITE_PROTECTED_OBJ(_streams);
        if (!_streams->count(ssrc)) {
            if (WebMCodecs::IsSupported(mime)) {
                auto stream = std::make_unique<RtpPacketsPlayerStream>(ssrc,
                                                                       mime,
                                                                       _timer,
                                                                       packetsInfoProvider,
                                                                       callback);
                _streams.Ref()[ssrc] = std::move(stream);
            }
            else {
                // TODO: log error
            }
        }
    }
}

void RtpPacketsPlayer::RemoveStream(uint32_t ssrc)
{
    if (ssrc) {
        LOCK_WRITE_PROTECTED_OBJ(_streams);
        _streams->erase(ssrc);
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

void RtpPacketsPlayer::Play(uint32_t ssrc, uint64_t mediaId,
                            const std::shared_ptr<MemoryBuffer>& buffer,
                            const void* userData)
{
    if (buffer && !buffer->IsEmpty()) {
        LOCK_READ_PROTECTED_OBJ(_streams);
        const auto it = _streams->find(ssrc);
        if (it != _streams->end()) {
            it->second->Play(mediaId, buffer, userData);
        }
    }
}

} // namespace RTC
