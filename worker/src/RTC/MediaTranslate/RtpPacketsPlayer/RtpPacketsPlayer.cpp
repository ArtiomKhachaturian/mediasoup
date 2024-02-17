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
    uint64_t GetMediaId() const { return _mediaId; }
    uint64_t GetMediaSourceId() const { return _mediaSourceId; }
    virtual void Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback) = 0;
protected:
    QueuedTask(QueuedTaskType type, uint64_t mediaId, uint64_t mediaSourceId);
private:
    const QueuedTaskType _type;
    const uint64_t _mediaId;
    const uint64_t _mediaSourceId;
};

class QueuedStartFinishTask : public QueuedTask
{
public:
    QueuedStartFinishTask(uint64_t mediaId, uint64_t mediaSourceId, bool start);
    // impl. of QueuedTask
    void Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback) final;
private:
    const bool _start;
};

class QueuedRtpPacketTask : public QueuedTask
{
public:
    QueuedRtpPacketTask(const RTC::Timestamp& timestampOffset, RTC::RtpPacket* packet,
                        uint64_t mediaId, uint64_t mediaSourceId);
    // impl. of QueuedTask
    void Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback) final;
private:
    const RTC::Timestamp _timestampOffset;
    std::unique_ptr<RTC::RtpPacket> _packet;
};

}
#endif

namespace RTC
{

#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
class RtpPacketsPlayer::StreamWrapper : private RtpPacketsPlayerCallback
{
    using TasksQueue = std::queue<std::unique_ptr<QueuedTask>>;
    // key is media ID
    using MediaTasks = absl::flat_hash_map<uint64_t, TasksQueue>;
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
    void ReplayTasks(TasksQueue queue);
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
    void OnPlay(const Timestamp& timestampOffset, RtpPacket* packet, uint64_t mediaId,
                uint64_t mediaSourceId) final;
    void OnPlayFinished(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
private:
    static void OnInvoke(uv_async_t* handle);
    void OnInvoke();
    void EnqueTask(std::unique_ptr<QueuedTask> task);
    bool HasCallback() const;
private:
    const UVAsyncHandle _handle;
    std::unique_ptr<RtpPacketsPlayerStream> _impl;
    ProtectedObj<RtpPacketsPlayerCallback*> _callback;
    // key is media source ID
    ProtectedObj<absl::flat_hash_map<uint64_t, MediaTasks>> _tasks;
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

void RtpPacketsPlayer::StreamWrapper::ReplayTasks(TasksQueue queue)
{
    LOCK_READ_PROTECTED_OBJ(_callback);
    if (const auto callback = _callback.ConstRef()) {
        while (!queue.empty()) {
            const auto& task = queue.front();
            task->Run(_impl->GetSsrc(), callback);
            queue.pop();
        }
    }
}

void RtpPacketsPlayer::StreamWrapper::OnPlayStarted(uint32_t ssrc, uint64_t mediaId,
                                                    uint64_t mediaSourceId)
{
    if (HasCallback()) {
        EnqueTask(std::make_unique<QueuedStartFinishTask>(mediaId, mediaSourceId, true));
    }
}

void RtpPacketsPlayer::StreamWrapper::OnPlay(const Timestamp& timestampOffset,
                                             RtpPacket* packet, uint64_t mediaId,
                                             uint64_t mediaSourceId)
{
    if (packet && HasCallback()) {
        EnqueTask(std::make_unique<QueuedRtpPacketTask>(timestampOffset, packet,
                                                        mediaId, mediaSourceId));
    }
}

void RtpPacketsPlayer::StreamWrapper::OnPlayFinished(uint32_t ssrc, uint64_t mediaId,
                                                     uint64_t mediaSourceId)
{
    if (HasCallback()) {
        EnqueTask(std::make_unique<QueuedStartFinishTask>(mediaId, mediaSourceId, false));
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
    std::list<uint64_t> completedMediaSourcesId;
    for (auto it = _tasks->begin(); it != _tasks->end(); ++it) {
        std::list<uint64_t> completedMediasId;
        for (auto itm = it->second.begin(); itm != it->second.end(); ++itm) {
            if (!itm->second.empty()) {
                const auto hasStart = QueuedTaskType::Start == itm->second.front()->GetType();
                if (hasStart) {
                    const auto hasFinish = QueuedTaskType::Finish == itm->second.back()->GetType();
                    if (hasFinish) { // batch is completed
                        ReplayTasks(std::move(itm->second));
                        completedMediasId.push_back(itm->first);
                    }
                }
            }
        }
        if (!completedMediasId.empty()) {
            for (const auto completedMediaId : completedMediasId) {
                it->second.erase(completedMediaId);
            }
            if (it->second.empty()) {
                completedMediaSourcesId.push_back(it->first);
            }
        }
    }
    for (const auto completedMediaSourceId : completedMediaSourcesId) {
        _tasks->erase(completedMediaSourceId);
    }
}

void RtpPacketsPlayer::StreamWrapper::EnqueTask(std::unique_ptr<QueuedTask> task)
{
    if (task) {
        const auto mediaSourceId = task->GetMediaSourceId();
        const auto mediaId = task->GetMediaId();
        const auto type = task->GetType();
        {
            LOCK_WRITE_PROTECTED_OBJ(_tasks);
            if (QueuedTaskType::Start == type) {
                auto it = _tasks->find(mediaSourceId);
                if (it == _tasks->end()) {
                    it = _tasks->insert(std::make_pair(mediaSourceId, MediaTasks())).first;
                }
                it->second[mediaId].push(std::move(task));
            }
            else {
                const auto it = _tasks->find(mediaSourceId);
                if (it != _tasks->end()) {
                    it->second[mediaId].push(std::move(task));
                }
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

QueuedTask::QueuedTask(QueuedTaskType type, uint64_t mediaId, uint64_t mediaSourceId)
    : _type(type)
    , _mediaId(mediaId)
    , _mediaSourceId(mediaSourceId)
{
}

QueuedStartFinishTask::QueuedStartFinishTask(uint64_t mediaId, uint64_t mediaSourceId, bool start)
    : QueuedTask(start ? QueuedTaskType::Start : QueuedTaskType::Finish, mediaId, mediaSourceId)
    , _start(start)
{
}

void QueuedStartFinishTask::Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback)
{
    if (callback) {
        if (_start) {
            callback->OnPlayStarted(ssrc, GetMediaId(), GetMediaSourceId());
        }
        else {
            callback->OnPlayFinished(ssrc, GetMediaId(), GetMediaSourceId());
        }
    }
}

QueuedRtpPacketTask::QueuedRtpPacketTask(const RTC::Timestamp& timestampOffset,
                                         RTC::RtpPacket* packet,
                                         uint64_t mediaId, uint64_t mediaSourceId)
    : QueuedTask(QueuedTaskType::Packet, mediaId, mediaSourceId)
    , _timestampOffset(timestampOffset)
    , _packet(packet)
{
}

void QueuedRtpPacketTask::Run(uint32_t ssrc, RTC::RtpPacketsPlayerCallback* callback)
{
    if (callback && _packet) {
        callback->OnPlay(_timestampOffset, _packet.release(), GetMediaId(), GetMediaSourceId());
    }
}

}
#endif
