#define MS_CLASS "RTC::RtpPacketsPlayerStreamQueue"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStreamQueue.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayerSimpleStreamQueue::RtpPacketsPlayerSimpleStreamQueue(uint32_t ssrc,
                                                                     uint32_t clockRate,
                                                                     uint8_t payloadType,
                                                                     const RtpCodecMimeType& mime)
    : _ssrc(ssrc)
    , _clockRate(clockRate)
    , _payloadType(payloadType)
    , _mime(mime)
{
}

RtpPacketsPlayerSimpleStreamQueue::~RtpPacketsPlayerSimpleStreamQueue()
{
    ResetCallback();
}

std::shared_ptr<RtpPacketsPlayerSimpleStreamQueue> RtpPacketsPlayerSimpleStreamQueue::
    Create(uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
           const RtpCodecMimeType& mime, RtpPacketsPlayerCallback* callback)
{
    std::shared_ptr<RtpPacketsPlayerSimpleStreamQueue> queue;
    if (callback) {
        if (WebMCodecs::IsSupported(mime)) {
            queue.reset(new RtpPacketsPlayerSimpleStreamQueue(ssrc, clockRate, payloadType, mime));
            queue->SetCallback(callback);
        }
        else {
            MS_ERROR("WebM unsupported MIME type %s", GetStreamInfoString(mime, ssrc).c_str());
        }
    }
    return queue;
}

void RtpPacketsPlayerSimpleStreamQueue::Play(uint64_t mediaSourceId,
                                             const std::shared_ptr<MemoryBuffer>& media,
                                             const std::shared_ptr<MediaTimer>& timer)
{
    if (media && timer && !media->IsEmpty()) {
        LOCK_READ_PROTECTED_OBJ(_callback);
        if (_callback.ConstRef()) {
            LOCK_WRITE_PROTECTED_OBJ(_pendingMedias);
            _pendingMedias->push(std::make_pair(mediaSourceId, media));
            if (!timer->Singleshot(0U, shared_from_this())) {
                MS_ERROR_STD("RTP media play not started");
                _pendingMedias->pop();
            }
        }
    }
}

bool RtpPacketsPlayerSimpleStreamQueue::IsEmpty() const
{
    LOCK_READ_PROTECTED_OBJ(_pendingMedias);
    return _pendingMedias->empty();
}

void RtpPacketsPlayerSimpleStreamQueue::OnEvent()
{
    LOCK_READ_PROTECTED_OBJ(_callback);
    if (const auto callback = _callback.ConstRef()) {
        LOCK_WRITE_PROTECTED_OBJ(_pendingMedias);
        while (!_pendingMedias->empty()) {
            const auto& pendingMedia = _pendingMedias->front();
            if (pendingMedia.second) {
                RtpPacketsPlayerMediaFragment fragment(std::make_unique<WebMDeserializer>());
                if (fragment.Parse(GetClockRate(), GetMime(), pendingMedia.second)) {
                    const auto mediaId = pendingMedia.second->GetId();
                    fragment.Play(GetSsrc(), GetPayloadType(), mediaId, pendingMedia.first, callback);
                }
            }
            _pendingMedias->pop();
        }
    }
}

void RtpPacketsPlayerSimpleStreamQueue::SetCallback(RtpPacketsPlayerCallback* callback)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_callback);
        if (callback != _callback.ConstRef()) {
            _callback = callback;
            changed = true;
        }
    }
    if (changed && !callback) {
        LOCK_WRITE_PROTECTED_OBJ(_pendingMedias);
        while (!_pendingMedias->empty()) {
            _pendingMedias->pop();
        }
    }
}

} // namespace RTC
