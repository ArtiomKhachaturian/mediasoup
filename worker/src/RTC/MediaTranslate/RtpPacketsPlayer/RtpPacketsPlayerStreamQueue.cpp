#define MS_CLASS "RTC::RtpPacketsPlayerStreamQueue"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStreamQueue.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayerStreamQueue::RtpPacketsPlayerStreamQueue(RtpPacketsPlayerCallback* callback)
    : _callback(callback)
{
    MS_ASSERT(callback, "non-null callback is expected");
}

RtpPacketsPlayerStreamQueue::~RtpPacketsPlayerStreamQueue()
{
    ResetCallback();
}

void RtpPacketsPlayerStreamQueue::ResetCallback()
{
    bool done = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_callback);
        if (_callback.ConstRef()) {
            _callback = nullptr;
            done = true;
        }
    }
    if (done) {
        LOCK_WRITE_PROTECTED_OBJ(_fragments);
        _fragments->clear();
    }
}

void RtpPacketsPlayerStreamQueue::PushFragment(std::unique_ptr<RtpPacketsPlayerMediaFragment> fragment)
{
    if (fragment) {
        const auto fragmentReference = fragment.get();
        LOCK_WRITE_PROTECTED_OBJ(_fragments);
        _fragments->insert(std::make_pair(fragmentReference->GetMediaId(), std::move(fragment)));
        fragmentReference->PlayFrames();
    }
}

bool RtpPacketsPlayerStreamQueue::HasFragments() const
{
    LOCK_READ_PROTECTED_OBJ(_fragments);
    return !_fragments->empty();
}

void RtpPacketsPlayerStreamQueue::OnPlayStarted(uint32_t ssrc, uint64_t mediaId, const void* userData)
{
    InvokeCallbackMethod(&RtpPacketsPlayerCallback::OnPlayStarted, ssrc, mediaId, userData);
}

void RtpPacketsPlayerStreamQueue::OnPlay(uint32_t rtpTimestampOffset, RtpPacket* packet,
                                         uint64_t mediaId, const void* userData)
{
    if (packet && !InvokeCallbackMethod(&RtpPacketsPlayerCallback::OnPlay,
                                        rtpTimestampOffset, packet,
                                        mediaId, userData)) {
        delete packet;
    }
}

void RtpPacketsPlayerStreamQueue::OnPlayFinished(uint32_t ssrc, uint64_t mediaId,
                                                 const void* userData)
{
    RemoveFinishedFragment(mediaId);
    InvokeCallbackMethod(&RtpPacketsPlayerCallback::OnPlayFinished, ssrc, mediaId, userData);
}

void RtpPacketsPlayerStreamQueue::RemoveFinishedFragment(uint64_t mediaId)
{
    LOCK_WRITE_PROTECTED_OBJ(_fragments);
    _fragments->erase(mediaId);
}

template <class Method, typename... Args>
bool RtpPacketsPlayerStreamQueue::InvokeCallbackMethod(const Method& method, Args&&... args) const
{
    LOCK_READ_PROTECTED_OBJ(_callback);
    if (const auto callback = _callback.ConstRef()) {
        (callback->*method)(std::forward<Args>(args)...);
        return true;
    }
    return false;
}

} // namespace RTC
