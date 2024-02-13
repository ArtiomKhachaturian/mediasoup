#define MS_CLASS "RTC::RtpPacketsPlayerStream"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/RtpPacketsInfoProvider.hpp"
#include "RTC/RtpPacket.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"

namespace RTC
{

class RtpPacketsPlayerStream::FragmentsQueue : public RtpPacketsPlayerCallback
{
    using FragmentsMap = absl::flat_hash_map<uint64_t, std::unique_ptr<RtpPacketsPlayerMediaFragment>>;
public:
    void SetCallback(RtpPacketsPlayerCallback* callback);
    void PushFragment(std::unique_ptr<RtpPacketsPlayerMediaFragment> fragment);
    bool HasFragments() const;
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint32_t ssrc, uint64_t mediaId, const void* userData) final;
    void OnPlay(uint32_t rtpTimestampOffset, RtpPacket* packet, uint64_t mediaId,
                const void* userData) final;
    void OnPlayFinished(uint32_t ssrc, uint64_t mediaId, const void* userData) final;
private:
    ProtectedObj<RtpPacketsPlayerCallback*> _callback = nullptr;
    ProtectedObj<FragmentsMap> _fragments;
};

RtpPacketsPlayerStream::RtpPacketsPlayerStream(uint32_t ssrc, const RtpCodecMimeType& mime,
                                               const std::shared_ptr<MediaTimer>& timer,
                                               const RtpPacketsInfoProvider* packetsInfoProvider,
                                               RtpPacketsPlayerCallback* callback)
    : _ssrc(ssrc)
    , _mime(mime)
    , _timer(timer)
    , _packetsInfoProvider(packetsInfoProvider)
    , _fragmentsQueue(std::make_shared<FragmentsQueue>())
{
    _fragmentsQueue->SetCallback(callback);
}

RtpPacketsPlayerStream::~RtpPacketsPlayerStream()
{
    _fragmentsQueue->SetCallback(nullptr);
}

void RtpPacketsPlayerStream::Play(uint64_t mediaId, const std::shared_ptr<MemoryBuffer>& buffer,
                                  const void* userData)
{
    if (buffer) {
        auto fragment = std::make_unique<RtpPacketsPlayerMediaFragment>(_timer, _fragmentsQueue,
                                                                        std::make_unique<WebMDeserializer>(),
                                                                        _ssrc, mediaId, userData);
        if (fragment->Parse(_mime, _packetsInfoProvider->GetClockRate(_ssrc), buffer)) {
            _fragmentsQueue->PushFragment(std::move(fragment));
        }
    }
}

bool RtpPacketsPlayerStream::IsPlaying() const
{
    return _fragmentsQueue->HasFragments();
}

void RtpPacketsPlayerStream::FragmentsQueue::SetCallback(RtpPacketsPlayerCallback* callback)
{
    LOCK_WRITE_PROTECTED_OBJ(_callback);
    if (callback != _callback) {
        _callback = callback;
        if (!callback) {
            LOCK_WRITE_PROTECTED_OBJ(_fragments);
            _fragments->clear();
        }
    }
}

void RtpPacketsPlayerStream::FragmentsQueue::PushFragment(std::unique_ptr<RtpPacketsPlayerMediaFragment> fragment)
{
    if (fragment) {
        const auto ref = fragment.get();
        LOCK_WRITE_PROTECTED_OBJ(_fragments);
        _fragments->insert(std::make_pair(ref->GetMediaId(), std::move(fragment)));
        ref->PlayFrames();
    }
}

bool RtpPacketsPlayerStream::FragmentsQueue::HasFragments() const
{
    LOCK_READ_PROTECTED_OBJ(_fragments);
    return !_fragments->empty();
}

void RtpPacketsPlayerStream::FragmentsQueue::OnPlayStarted(uint32_t ssrc, uint64_t mediaId,
                                                           const void* userData)
{
    LOCK_READ_PROTECTED_OBJ(_callback);
    if (const auto callback = _callback.ConstRef()) {
        callback->OnPlayStarted(ssrc, mediaId, userData);
    }
}

void RtpPacketsPlayerStream::FragmentsQueue::OnPlay(uint32_t rtpTimestampOffset,
                                                    RtpPacket* packet, uint64_t mediaId,
                                                    const void* userData)
{
    if (packet) {
        LOCK_READ_PROTECTED_OBJ(_callback);
        if (const auto callback = _callback.ConstRef()) {
            callback->OnPlay(rtpTimestampOffset, packet, mediaId, userData);
        }
        else {
            delete packet;
        }
    }
}

void RtpPacketsPlayerStream::FragmentsQueue::OnPlayFinished(uint32_t ssrc, uint64_t mediaId,
                                                            const void* userData)
{
    {
        LOCK_READ_PROTECTED_OBJ(_callback);
        if (const auto callback = _callback.ConstRef()) {
            callback->OnPlayFinished(ssrc, mediaId, userData);
        }
    }
    LOCK_WRITE_PROTECTED_OBJ(_fragments);
    _fragments->erase(mediaId);
}

} // namespace RTC
