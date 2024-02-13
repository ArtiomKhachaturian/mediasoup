#define MS_CLASS "RTC::RtpPacketsPlayer"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
#include "RTC/RtpPacket.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "LibUVAsyncHandle.hpp"
#include "ProtectedObj.hpp"
#endif
#include "MemoryBuffer.hpp"
#include "Logger.hpp"
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
#include <queue>
#endif


#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
namespace {

class QueuedTask
{
public:
    virtual ~QueuedTask() = default;
    virtual void Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback) = 0;
protected:
    QueuedTask(uint64_t mediaId, const void* userData);
    uint64_t GetMediaId() const { return _mediaId; }
    const void* GetUserData() const { return _userData; }
private:
    const uint64_t _mediaId;
    const void* const _userData;
};

class QueuedStartFinishTask : public QueuedTask
{
public:
    QueuedStartFinishTask(uint64_t mediaId, const void* userData, bool start);
    // impl. of QueuedTask
    void Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback) final;
private:
    const bool _start;
};

class QueuedRtpPacketTask : public QueuedTask
{
public:
    QueuedRtpPacketTask(uint32_t rtpTimestampOffset, RTC::RtpPacket* packet,
                        uint64_t mediaId, const void* userData);
    ~QueuedRtpPacketTask() final;
    // impl. of QueuedTask
    void Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback) final;
private:
    const uint32_t _rtpTimestampOffset;
    RTC::RtpPacket* _packet;
};

}
#endif

namespace RTC
{

#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
class RtpPacketsPlayer::StreamWrapper : private RtpPacketsPlayerCallback
{
public:
    StreamWrapper(uint32_t ssrc, const RtpCodecMimeType& mime,
                  const std::shared_ptr<MediaTimer>& timer,
                  const RtpPacketsInfoProvider* packetsInfoProvider,
                  RtpPacketsPlayerCallback* callback);
    void ResetCallback();
    void Play(uint64_t mediaId, const std::shared_ptr<MemoryBuffer>& buffer, const void* userData);
    bool IsPlaying() const;
private:
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint32_t ssrc, uint64_t mediaId, const void* userData) final;
    void OnPlay(uint32_t rtpTimestampOffset, RtpPacket* packet, uint64_t mediaId,
                const void* userData) final;
    void OnPlayFinished(uint32_t ssrc, uint64_t mediaId, const void* userData) final;
private:
    static void OnInvoke(uv_async_t* handle);
    void OnInvoke();
    void EnqueTask(std::unique_ptr<QueuedTask> task);
    bool HasCallback() const;
private:
    const LibUVAsyncHandle _handle;
    const std::unique_ptr<RtpPacketsPlayerStream> _stream;
    ProtectedObj<RtpPacketsPlayerCallback*> _callback;
    ProtectedObj<std::queue<std::unique_ptr<QueuedTask>>> _tasks;
};
#endif

RtpPacketsPlayer::RtpPacketsPlayer()
    : _timer(std::make_shared<MediaTimer>("RtpPacketsPlayer"))
{
}

RtpPacketsPlayer::~RtpPacketsPlayer()
{
    LOCK_WRITE_PROTECTED_OBJ(_streams);
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
    for (auto it = _streams->begin(); it != _streams->end(); ++it) {
        it->second->ResetCallback();
    }
#endif
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
                auto stream = std::make_unique<StreamType>(ssrc, mime, _timer,
                                                           packetsInfoProvider, callback);
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
        const auto it = _streams->find(ssrc);
        if (it != _streams->end()) {
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
            it->second->ResetCallback();
#endif
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

#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
RtpPacketsPlayer::StreamWrapper::StreamWrapper(uint32_t ssrc, const RtpCodecMimeType& mime,
                                               const std::shared_ptr<MediaTimer>& timer,
                                               const RtpPacketsInfoProvider* packetsInfoProvider,
                                               RtpPacketsPlayerCallback* callback)
    : _handle(OnInvoke, this)
    , _stream(new RtpPacketsPlayerStream(ssrc, mime, timer, packetsInfoProvider, this))
    , _callback(callback)
{
    MS_ASSERT(_handle.IsValid(), "invalid async handle");
}

void RtpPacketsPlayer::StreamWrapper::ResetCallback()
{
    LOCK_WRITE_PROTECTED_OBJ(_callback);
    if (_callback) {
        _callback = nullptr;
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        while (!_tasks->empty()) {
            _tasks->pop();
        }
    }
}

void RtpPacketsPlayer::StreamWrapper::Play(uint64_t mediaId,
                                           const std::shared_ptr<MemoryBuffer>& buffer,
                                           const void* userData)
{
    _stream->Play(mediaId, buffer, userData);
}

bool RtpPacketsPlayer::StreamWrapper::IsPlaying() const
{
    return _stream->IsPlaying();
}

void RtpPacketsPlayer::StreamWrapper::OnPlayStarted(uint32_t ssrc, uint64_t mediaId,
                                                    const void* userData)
{
    if (HasCallback()) {
        EnqueTask(std::make_unique<QueuedStartFinishTask>(mediaId, userData, true));
    }
}

void RtpPacketsPlayer::StreamWrapper::OnPlay(uint32_t rtpTimestampOffset,
                                             RtpPacket* packet,
                                             uint64_t mediaId,
                                             const void* userData)
{
    if (packet && HasCallback()) {
        EnqueTask(std::make_unique<QueuedRtpPacketTask>(rtpTimestampOffset, packet,
                                                        mediaId, userData));
    }
}

void RtpPacketsPlayer::StreamWrapper::OnPlayFinished(uint32_t ssrc, uint64_t mediaId,
                                                     const void* userData)
{
    if (HasCallback()) {
        EnqueTask(std::make_unique<QueuedStartFinishTask>(mediaId, userData, false));
    }
}

void RtpPacketsPlayer::StreamWrapper::OnInvoke(uv_async_t* handle)
{
    if (handle && handle->data) {
        reinterpret_cast<StreamWrapper*>(handle->data)->OnInvoke();
    }
}

void RtpPacketsPlayer::StreamWrapper::OnInvoke()
{
    LOCK_READ_PROTECTED_OBJ(_callback);
    if (const auto callback = _callback.ConstRef()) {
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        if (!_tasks->empty()) {
            auto task = std::move(_tasks->front());
            _tasks->pop();
            if (task) {
                task->Run(_stream->GetSsrc(), callback);
            }
        }
    }
}

void RtpPacketsPlayer::StreamWrapper::EnqueTask(std::unique_ptr<QueuedTask> task)
{
    if (task) {
        {
            LOCK_WRITE_PROTECTED_OBJ(_tasks);
            _tasks->push(std::move(task));
        }
        _handle.Invoke();
    }
}

bool RtpPacketsPlayer::StreamWrapper::HasCallback() const
{
    LOCK_READ_PROTECTED_OBJ(_callback);
    return nullptr != _callback.ConstRef();
}

#endif
} // namespace RTC

#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
namespace {

QueuedTask::QueuedTask(uint64_t mediaId, const void* userData)
    : _mediaId(mediaId)
    , _userData(userData)
{
}

QueuedStartFinishTask::QueuedStartFinishTask(uint64_t mediaId, const void* userData, bool start)
    : QueuedTask(mediaId, userData)
    , _start(start)
{
}

void QueuedStartFinishTask::Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback)
{
    if (callback) {
        if (_start) {
            callback->OnPlayStarted(ssrc, GetMediaId(), GetUserData());
        }
        else {
            callback->OnPlayFinished(ssrc, GetMediaId(), GetUserData());
        }
    }
}

QueuedRtpPacketTask::QueuedRtpPacketTask(uint32_t rtpTimestampOffset, RTC::RtpPacket* packet,
                                           uint64_t mediaId, const void* userData)
    : QueuedTask(mediaId, userData)
    , _rtpTimestampOffset(rtpTimestampOffset)
    , _packet(packet)
{
}

QueuedRtpPacketTask::~QueuedRtpPacketTask()
{
    delete _packet;
}

void QueuedRtpPacketTask::Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback)
{
    if (callback && _packet) {
        callback->OnPlay(_rtpTimestampOffset, _packet, GetMediaId(), GetUserData());
        _packet = nullptr;
    }
}

}
#endif
