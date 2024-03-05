#define MS_CLASS "RTC::RtpPacketsPlayerMainLoopStream"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMainLoopStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerSimpleStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Timestamp.hpp"
#include "UVAsyncHandle.hpp"
#include "ProtectedObj.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include <atomic>
#include <unordered_map>
#include <queue>

namespace {

using namespace RTC;

enum class TaskType {
    Start,
    Finish,
    Packet
};

class Task
{
public:
    virtual ~Task() = default;
    TaskType GetType() const { return _type; }
    bool IsStartTask() const { return TaskType::Start == GetType(); }
    bool IsFinishTask() const { return TaskType::Finish == GetType(); }
    virtual void Run(uint64_t mediaId, uint64_t mediaSourceId,
                     RtpPacketsPlayerCallback* callback) = 0;
protected:
    Task(TaskType type);
private:
    const TaskType _type;
};

class StartFinishTask : public Task
{
public:
    StartFinishTask(uint32_t ssrc, bool start);
    // impl. of QueuedTask
    void Run(uint64_t mediaId, uint64_t mediaSourceId, RtpPacketsPlayerCallback* callback) final;
private:
    const uint32_t _ssrc;
};

class RtpPacketTask : public Task
{
public:
    RtpPacketTask(RtpTranslatedPacket packet);
    // impl. of QueuedTask
    void Run(uint64_t mediaId, uint64_t mediaSourceId, RtpPacketsPlayerCallback* callback) final;
private:
    RtpTranslatedPacket _packet;
};

class QueuedMediaTasks
{
public:
    QueuedMediaTasks() = default;
    void Enqueue(std::unique_ptr<Task> task);
    // return true if finished task was played
    bool Play(uint64_t mediaId, uint64_t mediaSourceId, RtpPacketsPlayerCallback* callback);
private:
    std::queue<std::unique_ptr<Task>> _tasks;
};

class QueuedMediaSouceTasks
{
public:
    QueuedMediaSouceTasks() = default;
    void Enqueue(uint64_t mediaId, std::unique_ptr<Task> task);
    void Play(uint64_t mediaSourceId, RtpPacketsPlayerCallback* callback);
    bool IsEmpty() const { return _mediaTasks.empty(); }
private:
    // key is media ID
    std::unordered_map<uint64_t, QueuedMediaTasks> _mediaTasks;
};

}

namespace RTC
{

class RtpPacketsPlayerMainLoopStream::Impl : public RtpPacketsPlayerCallback
{
public:
    Impl(RtpPacketsPlayerCallback* callback);
    ~Impl() final { ClearTasks(); }
    void Deactivate();
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc) final;
    void OnPlay(uint64_t mediaId, uint64_t mediaSourceId, RtpTranslatedPacket packet) final;
    void OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc) final;
private:
    static void OnInvoke(uv_async_t* handle);
    void OnInvoke();
    void ClearTasks();
    void EnqueTask(uint64_t mediaId, uint64_t mediaSourceId, std::unique_ptr<Task> task);
    bool IsActive() const { return _active.load(); }
private:
    const UVAsyncHandle _handle;
    RtpPacketsPlayerCallback* const _callback;
    // key is media source ID
    ProtectedObj<std::unordered_map<uint64_t, QueuedMediaSouceTasks>> _tasks;
    std::atomic_bool _active = true;
};

RtpPacketsPlayerMainLoopStream::RtpPacketsPlayerMainLoopStream(std::unique_ptr<Impl> impl,
                                                               std::unique_ptr<RtpPacketsPlayerStream> simpleStream)
    : _impl(std::move(impl))
    , _simpleStream(std::move(simpleStream))
{
}

RtpPacketsPlayerMainLoopStream::~RtpPacketsPlayerMainLoopStream()
{
    _impl->Deactivate();
}

std::unique_ptr<RtpPacketsPlayerStream> RtpPacketsPlayerMainLoopStream::
    Create(uint32_t ssrc, uint32_t clockRate,
           uint8_t payloadType,
           const RtpCodecMimeType& mime,
           RtpPacketsPlayerCallback* callback,
           const std::shared_ptr<BufferAllocator>& allocator)
{
    std::unique_ptr<RtpPacketsPlayerStream> stream;
    if (callback) {
        auto impl = std::make_unique<Impl>(callback);
        if (auto simpleStream = RtpPacketsPlayerSimpleStream::Create(ssrc, clockRate,
                                                                     payloadType, mime,
                                                                     impl.get(), allocator)) {
            stream.reset(new RtpPacketsPlayerMainLoopStream(std::move(impl), std::move(simpleStream)));
        }
    }
    return stream;
}

void RtpPacketsPlayerMainLoopStream::Play(uint64_t mediaSourceId,
                                          const std::shared_ptr<Buffer>& media,
                                          const std::shared_ptr<MediaTimer> timer)
{
    _simpleStream->Play(mediaSourceId, media, timer);
}

void RtpPacketsPlayerMainLoopStream::Stop(uint64_t mediaSourceId, uint64_t mediaId)
{
    _simpleStream->Stop(mediaSourceId, mediaId);
}

bool RtpPacketsPlayerMainLoopStream::IsPlaying() const
{
    return _simpleStream->IsPlaying();
}

RtpPacketsPlayerMainLoopStream::Impl::Impl(RtpPacketsPlayerCallback* callback)
    : _handle(DepLibUV::GetLoop(), OnInvoke, this)
    , _callback(callback)
{
    _tasks->reserve(1U);
}

void RtpPacketsPlayerMainLoopStream::Impl::Deactivate()
{
    if (_active.exchange(false)) {
        ClearTasks();
    }
}

void RtpPacketsPlayerMainLoopStream::Impl::OnPlayStarted(uint64_t mediaId,
                                                         uint64_t mediaSourceId,
                                                         uint32_t ssrc)
{
    if (IsActive()) {
        EnqueTask(mediaId, mediaSourceId, std::make_unique<StartFinishTask>(ssrc, true));
    }
}

void RtpPacketsPlayerMainLoopStream::Impl::OnPlay(uint64_t mediaId, uint64_t mediaSourceId,
                                                  RtpTranslatedPacket packet)
{
    if (IsActive()) {
        EnqueTask(mediaId, mediaSourceId,  std::make_unique<RtpPacketTask>(std::move(packet)));
    }
}

void RtpPacketsPlayerMainLoopStream::Impl::OnPlayFinished(uint64_t mediaId,
                                                          uint64_t mediaSourceId,
                                                          uint32_t ssrc)
{
    if (IsActive()) {
        EnqueTask(mediaId, mediaSourceId, std::make_unique<StartFinishTask>(ssrc, false));
    }
}

void RtpPacketsPlayerMainLoopStream::Impl::OnInvoke(uv_async_t* handle)
{
    if (handle && handle->data) {
        reinterpret_cast<Impl*>(handle->data)->OnInvoke();
    }
}

void RtpPacketsPlayerMainLoopStream::Impl::OnInvoke()
{
    if (IsActive()) {
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        if (!_tasks->empty()) {
            std::list<uint64_t> completedMediaSources;
            for (auto it = _tasks->begin(); it != _tasks->end(); ++it) {
                it->second.Play(it->first, _callback);
                if (!it->second.IsEmpty()) {
                    completedMediaSources.push_back(it->first);
                }
            }
            for (const auto completedMediaSource : completedMediaSources) {
                _tasks->erase(completedMediaSource);
            }
        }
    }
}

void RtpPacketsPlayerMainLoopStream::Impl::ClearTasks()
{
    LOCK_WRITE_PROTECTED_OBJ(_tasks);
    _tasks->clear();
}

void RtpPacketsPlayerMainLoopStream::Impl::EnqueTask(uint64_t mediaId, uint64_t mediaSourceId,
                                                     std::unique_ptr<Task> task)
{
    if (task && IsActive()) {
        {
            LOCK_WRITE_PROTECTED_OBJ(_tasks);
            _tasks.Ref()[mediaSourceId].Enqueue(mediaId, std::move(task));
        }
        _handle.Invoke();
    }
}

} // namespace RTC

namespace {

Task::Task(TaskType type)
    : _type(type)
{
}

StartFinishTask::StartFinishTask(uint32_t ssrc, bool start)
    : Task(start ? TaskType::Start : TaskType::Finish)
    , _ssrc(ssrc)
{
}

void StartFinishTask::Run(uint64_t mediaId, uint64_t mediaSourceId, RtpPacketsPlayerCallback* callback)
{
    if (callback) {
        if (TaskType::Start == GetType()) {
            callback->OnPlayStarted(mediaId, mediaSourceId, _ssrc);
        }
        else {
            callback->OnPlayFinished(mediaId, mediaSourceId, _ssrc);
        }
    }
}

RtpPacketTask::RtpPacketTask(RtpTranslatedPacket packet)
    : Task(TaskType::Packet)
    , _packet(std::move(packet))
{
}

void RtpPacketTask::Run(uint64_t mediaId, uint64_t mediaSourceId, RtpPacketsPlayerCallback* callback)
{
    if (callback) {
        callback->OnPlay(mediaId, mediaSourceId, std::move(_packet));
    }
}

void QueuedMediaTasks::Enqueue(std::unique_ptr<Task> task)
{
    if (task) {
        _tasks.push(std::move(task));
    }
}

bool QueuedMediaTasks::Play(uint64_t mediaId, uint64_t mediaSourceId,
                            RtpPacketsPlayerCallback* callback)
{
    bool finished = false;
    while (!_tasks.empty()) {
        const auto& task = _tasks.front();
        if (!finished) {
            task->Run(mediaId, mediaSourceId, callback);
        }
        if (TaskType::Finish == task->GetType()) {
            finished = true;
        }
        _tasks.pop();
    }
    return finished;
}

void QueuedMediaSouceTasks::Enqueue(uint64_t mediaId, std::unique_ptr<Task> task)
{
    if (task) {
        _mediaTasks[mediaId].Enqueue(std::move(task));
    }
}

void QueuedMediaSouceTasks::Play(uint64_t mediaSourceId, RtpPacketsPlayerCallback* callback)
{
    if (!IsEmpty()) {
        std::list<uint64_t> completedMedias;
        for (auto it = _mediaTasks.begin(); it != _mediaTasks.end(); ++it) {
            if (it->second.Play(it->first, mediaSourceId, callback)) {
                completedMedias.push_back(it->first);
            }
        }
        for (const auto completedMedia : completedMedias) {
            _mediaTasks.erase(completedMedia);
        }
    }
}

}
