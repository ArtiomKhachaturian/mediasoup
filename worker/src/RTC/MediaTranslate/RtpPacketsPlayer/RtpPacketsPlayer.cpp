#define MS_CLASS "RTC::RtpPacketsPlayer"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
#include "RTC/RtpPacket.hpp"
#include "RTC/Timestamp.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "UVAsyncHandle.hpp"
#include "ProtectedObj.hpp"
#include "DepLibUV.hpp"
#endif
#include "Logger.hpp"
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
#include <queue>
#endif


#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
namespace {

enum class QueuedTaskType {
    Start,
    Finish,
    Packet
};

class QueuedTask
{
public:
    virtual ~QueuedTask() = default;
    QueuedTaskType GetType() const { return _type; }
    bool IsStartTask() const { return QueuedTaskType::Start == GetType(); }
    bool IsFinishTask() const { return QueuedTaskType::Finish == GetType(); }
    virtual void Run(uint64_t mediaId, uint64_t mediaSourceId,
                     RTC::RtpPacketsPlayerCallback* callback) = 0;
protected:
    QueuedTask(QueuedTaskType type);
private:
    const QueuedTaskType _type;
};

class QueuedStartFinishTask : public QueuedTask
{
public:
    QueuedStartFinishTask(uint32_t ssrc, bool start);
    // impl. of QueuedTask
    void Run(uint64_t mediaId, uint64_t mediaSourceId, RTC::RtpPacketsPlayerCallback* callback) final;
private:
    const uint32_t _ssrc;
};

class QueuedRtpPacketTask : public QueuedTask
{
public:
    QueuedRtpPacketTask(const RTC::Timestamp& timestampOffset, RTC::RtpPacket* packet);
    // impl. of QueuedTask
    void Run(uint64_t mediaId, uint64_t mediaSourceId, RTC::RtpPacketsPlayerCallback* callback) final;
private:
    const RTC::Timestamp _timestampOffset;
    std::unique_ptr<RTC::RtpPacket> _packet;
};

class QueuedMediaTasks
{
public:
    QueuedMediaTasks() = default;
    bool IsReadyForPlay() const;
    void Enqueue(std::unique_ptr<QueuedTask> task);
    void Play(uint64_t mediaId, uint64_t mediaSourceId, RTC::RtpPacketsPlayerCallback* callback);
private:
    std::queue<std::unique_ptr<QueuedTask>> _tasks;
};

class QueuedMediaSouceTasks
{
public:
    QueuedMediaSouceTasks() = default;
    void Enqueue(uint64_t mediaId, std::unique_ptr<QueuedTask> task);
    void Play(uint64_t mediaSourceId, RTC::RtpPacketsPlayerCallback* callback);
    bool IsEmpty() const { return _mediaTasks.empty(); }
private:
    // key is media ID
    absl::flat_hash_map<uint64_t, QueuedMediaTasks> _mediaTasks;
};

}
#endif

namespace RTC
{

#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
class RtpPacketsPlayer::StreamWrapper : private RtpPacketsPlayerCallback
{
public:
    ~StreamWrapper() final;
    static std::unique_ptr<StreamWrapper> Create(uint32_t ssrc, uint32_t clockRate,
                                                 uint8_t payloadType,
                                                 const RtpCodecMimeType& mime,
                                                 RtpPacketsPlayerCallback* callback);
    void Play(uint64_t mediaSourceId, const std::shared_ptr<MemoryBuffer>& media,
              const std::shared_ptr<MediaTimer>& timer);
    bool IsPlaying() const;
private:
    StreamWrapper(RtpPacketsPlayerCallback* callback);
    void ResetCallback();
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
    void OnPlay(const Timestamp& timestampOffset, RtpPacket* packet, uint64_t mediaId,
                uint64_t mediaSourceId) final;
    void OnPlayFinished(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
private:
    static void OnInvoke(uv_async_t* handle);
    void OnInvoke();
    void EnqueTask(uint64_t mediaId, uint64_t mediaSourceId, std::unique_ptr<QueuedTask> task);
    bool HasCallback() const;
private:
    const UVAsyncHandle _handle;
    std::unique_ptr<RtpPacketsPlayerStream> _impl;
    ProtectedObj<RtpPacketsPlayerCallback*> _callback;
    // key is media source ID
    ProtectedObj<absl::flat_hash_map<uint64_t, QueuedMediaSouceTasks>> _tasks;
};
#endif

RtpPacketsPlayer::RtpPacketsPlayer()
    : _timer(std::make_shared<MediaTimer>("RtpPacketsPlayer"))
{
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
            if (auto stream = StreamType::Create(ssrc, clockRate, payloadType, mime, callback)) {
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

#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
RtpPacketsPlayer::StreamWrapper::StreamWrapper(RtpPacketsPlayerCallback* callback)
    : _handle(DepLibUV::GetLoop(), OnInvoke, this)
    , _callback(callback)
{
}

RtpPacketsPlayer::StreamWrapper::~StreamWrapper()
{
    ResetCallback();
    LOCK_WRITE_PROTECTED_OBJ(_tasks);
    _tasks->clear();
}

std::unique_ptr<RtpPacketsPlayer::StreamWrapper> RtpPacketsPlayer::StreamWrapper::
    Create(uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
           const RtpCodecMimeType& mime, RtpPacketsPlayerCallback* callback)
{
    if (callback) {
        std::unique_ptr<StreamWrapper> wrapper(new StreamWrapper(callback));
        if (auto impl = RtpPacketsPlayerStream::Create(ssrc, clockRate, payloadType, mime,
                                                       wrapper.get())) {
            wrapper->_impl = std::move(impl);
        }
        else {
            wrapper.reset();
        }
        return wrapper;
    }
    return nullptr;
}

void RtpPacketsPlayer::StreamWrapper::Play(uint64_t mediaSourceId,
                                           const std::shared_ptr<MemoryBuffer>& media,
                                           const std::shared_ptr<MediaTimer>& timer)
{
    if (_impl) {
        _impl->Play(mediaSourceId, media, timer);
    }
}

bool RtpPacketsPlayer::StreamWrapper::IsPlaying() const
{
    return _impl && _impl->IsPlaying();
}

void RtpPacketsPlayer::StreamWrapper::ResetCallback()
{
    LOCK_WRITE_PROTECTED_OBJ(_callback);
    _callback = nullptr;
}

void RtpPacketsPlayer::StreamWrapper::OnPlayStarted(uint32_t ssrc, uint64_t mediaId,
                                                    uint64_t mediaSourceId)
{
    if (HasCallback()) {
        auto task = std::make_unique<QueuedStartFinishTask>(ssrc, true);
        EnqueTask(mediaId, mediaSourceId, std::move(task));
    }
}

void RtpPacketsPlayer::StreamWrapper::OnPlay(const Timestamp& timestampOffset,
                                             RtpPacket* packet, uint64_t mediaId,
                                             uint64_t mediaSourceId)
{
    if (packet && HasCallback()) {
        auto task = std::make_unique<QueuedRtpPacketTask>(timestampOffset, packet);
        EnqueTask(mediaId, mediaSourceId, std::move(task));
    }
}

void RtpPacketsPlayer::StreamWrapper::OnPlayFinished(uint32_t ssrc, uint64_t mediaId,
                                                     uint64_t mediaSourceId)
{
    if (HasCallback()) {
        auto task = std::make_unique<QueuedStartFinishTask>(ssrc, false);
        EnqueTask(mediaId, mediaSourceId, std::move(task));
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
    LOCK_WRITE_PROTECTED_OBJ(_tasks);
    if (!_tasks->empty()) {
        std::list<uint64_t> completedMediaSources;
        {
            LOCK_READ_PROTECTED_OBJ(_callback);
            for (auto it = _tasks->begin(); it != _tasks->end(); ++it) {
                it->second.Play(it->first, _callback.ConstRef());
                if (!it->second.IsEmpty()) {
                    completedMediaSources.push_back(it->first);
                }
            }
        }
        for (const auto completedMediaSource : completedMediaSources) {
            _tasks->erase(completedMediaSource);
        }
    }
}

void RtpPacketsPlayer::StreamWrapper::EnqueTask(uint64_t mediaId, uint64_t mediaSourceId,
                                                std::unique_ptr<QueuedTask> task)
{
    if (task) {
        const auto type = task->GetType();
        {
            LOCK_WRITE_PROTECTED_OBJ(_tasks);
            auto it = _tasks->end();
            if (QueuedTaskType::Start == type) {
                it = _tasks->find(mediaSourceId);
                if (it == _tasks->end()) {
                    it = _tasks->insert(std::make_pair(mediaSourceId, QueuedMediaSouceTasks())).first;
                }
            }
            else {
                it = _tasks->find(mediaSourceId);
            }
            if (it != _tasks->end()) {
                it->second.Enqueue(mediaId, std::move(task));
            }
        }
        if (QueuedTaskType::Finish == type) {
            _handle.Invoke();
        }
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

QueuedTask::QueuedTask(QueuedTaskType type)
    : _type(type)
{
}

QueuedStartFinishTask::QueuedStartFinishTask(uint32_t ssrc, bool start)
    : QueuedTask(start ? QueuedTaskType::Start : QueuedTaskType::Finish)
    , _ssrc(ssrc)
{
}

void QueuedStartFinishTask::Run(uint64_t mediaId, uint64_t mediaSourceId,
                                RTC::RtpPacketsPlayerCallback* callback)
{
    if (callback) {
        if (QueuedTaskType::Start == GetType()) {
            callback->OnPlayStarted(_ssrc, mediaId, mediaSourceId);
        }
        else {
            callback->OnPlayFinished(_ssrc, mediaId, mediaSourceId);
        }
    }
}

QueuedRtpPacketTask::QueuedRtpPacketTask(const RTC::Timestamp& timestampOffset,
                                         RTC::RtpPacket* packet)
    : QueuedTask(QueuedTaskType::Packet)
    , _timestampOffset(timestampOffset)
    , _packet(packet)
{
}

void QueuedRtpPacketTask::Run(uint64_t mediaId, uint64_t mediaSourceId,
                              RTC::RtpPacketsPlayerCallback* callback)
{
    if (callback && _packet) {
        callback->OnPlay(_timestampOffset, _packet.release(), mediaId, mediaSourceId);
    }
}

bool QueuedMediaTasks::IsReadyForPlay() const
{
    if (!_tasks.empty()) {
        return _tasks.front()->IsStartTask() && _tasks.back()->IsFinishTask();
    }
    return false;
}

void QueuedMediaTasks::Enqueue(std::unique_ptr<QueuedTask> task)
{
    if (task) {
        if (_tasks.empty()) {
            MS_ASSERT(task->IsStartTask(), "The 1st task in the queue must be the start task");
        }
        _tasks.push(std::move(task));
    }
}

void QueuedMediaTasks::Play(uint64_t mediaId, uint64_t mediaSourceId,
                            RTC::RtpPacketsPlayerCallback* callback)
{
    while (!_tasks.empty()) {
        _tasks.front()->Run(mediaId, mediaSourceId, callback);
        _tasks.pop();
    }
}

void QueuedMediaSouceTasks::Enqueue(uint64_t mediaId, std::unique_ptr<QueuedTask> task)
{
    if (task) {
        _mediaTasks[mediaId].Enqueue(std::move(task));
    }
}

void QueuedMediaSouceTasks::Play(uint64_t mediaSourceId, RTC::RtpPacketsPlayerCallback* callback)
{
    if (!IsEmpty()) {
        std::list<uint64_t> completedMedias;
        for (auto it = _mediaTasks.begin(); it != _mediaTasks.end(); ++it) {
            if (it->second.IsReadyForPlay()) {
                it->second.Play(it->first, mediaSourceId, callback);
                completedMedias.push_back(it->first);
            }
        }
        for (const auto completedMedia : completedMedias) {
            _mediaTasks.erase(completedMedia);
        }
    }
}

}
#endif
