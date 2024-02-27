#define MS_CLASS "RTC::RtpPacketsPlayerMediaFragment"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Buffers/Buffer.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Timestamp.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include <atomic>
#include <queue>

namespace {

using namespace RTC;

enum class TaskType {
    Start,
    Frame
};

class Task
{
public:
    virtual ~Task() = default;
    TaskType GetType() const { return _type; }
    virtual std::optional<RtpTranslatedPacket> TakePacket() { return std::nullopt; }
protected:
    Task(TaskType type);
private:
    const TaskType _type;
};

class StartTask : public Task
{
public:
    StartTask(size_t trackIndex, const RtpCodecMimeType& mime,
              uint32_t ssrc, uint32_t clockRate,
              uint64_t mediaId, uint64_t mediaSourceId);
    size_t GetTrackIndex() const { return _trackIndex; }
    const RtpCodecMimeType& GetMime() const { return _mime; }
    uint32_t GetSsrc() const { return _ssrc; }
    uint64_t GetMediaId() const { return _mediaId; }
    uint64_t GetMediaSourceId() const { return _mediaSourceId; }
    const Timestamp& GeTimestamp() const { return _timestamp; }
    void SetTimestamp(Timestamp timestamp) { _timestamp = std::move(timestamp); }
private:
    const size_t _trackIndex;
    const RtpCodecMimeType _mime;
    const uint32_t _ssrc;
    const uint64_t _mediaId;
    const uint64_t _mediaSourceId;
    Timestamp _timestamp;
};

class MediaFrameTask : public Task
{
public:
    MediaFrameTask(std::optional<RtpTranslatedPacket> packet);
    // override of Task
    std::optional<RtpTranslatedPacket> TakePacket() final { return std::move(_packet); }
private:
    std::optional<RtpTranslatedPacket> _packet;
};

}

namespace RTC
{

class RtpPacketsPlayerMediaFragment::TasksQueue : public MediaTimerCallback
{
public:
    TasksQueue(const std::weak_ptr<MediaTimer> timerRef,
               std::unique_ptr<MediaFrameDeserializer> deserializer,
               RtpPacketsPlayerCallback* callback);
    ~TasksQueue() final;
    void SetTimerId(uint64_t timerId);
    void Start(size_t trackIndex, uint32_t ssrc, uint32_t clockRate,
               uint64_t mediaId, uint64_t mediaSourceId);
    size_t GetTracksCount() const;
    std::optional<RtpCodecMimeType> GetTrackType(size_t trackIndex) const;
    // impl. of MediaTimerCallback
    void OnEvent(uint64_t timerId) final;
private:
    uint64_t GetTimerId() const { return _timerId.load(); }
    void SetClockRate(size_t trackIndex, uint32_t clockRate);
    void Enque(std::unique_ptr<Task> task);
    void Process(std::unique_ptr<Task> task);
    void Process(StartTask* startTask, std::optional<RtpTranslatedPacket> packet);
    bool ReadNextFrame(StartTask* startTask, bool enque);
    void ClearTasks();
private:
    const std::weak_ptr<MediaTimer> _timerRef;
    const ProtectedUniquePtr<MediaFrameDeserializer> _deserializer;
    RtpPacketsPlayerCallback* const _callback;
    ProtectedUniquePtr<StartTask> _startTask;
    std::atomic<uint64_t> _timerId = 0;
    // used for adjust of timer interval
    ProtectedObj<std::queue<std::unique_ptr<Task>>> _tasks;
};

RtpPacketsPlayerMediaFragment::RtpPacketsPlayerMediaFragment(std::shared_ptr<TasksQueue> queue)
    : _queue(queue)
{
}

RtpPacketsPlayerMediaFragment::~RtpPacketsPlayerMediaFragment()
{
    _queue->SetTimerId(0ULL);
}

void RtpPacketsPlayerMediaFragment::Start(size_t trackIndex, uint32_t ssrc, uint32_t clockRate,
                                          uint64_t mediaId, uint64_t mediaSourceId)
{
    _queue->Start(trackIndex, ssrc, clockRate, mediaId, mediaSourceId);
}

size_t RtpPacketsPlayerMediaFragment::GetTracksCount() const
{
    return _queue->GetTracksCount();
}

std::optional<RtpCodecMimeType> RtpPacketsPlayerMediaFragment::GetTrackMimeType(size_t trackIndex) const
{
    return _queue->GetTrackType(trackIndex);
}

std::unique_ptr<RtpPacketsPlayerMediaFragment> RtpPacketsPlayerMediaFragment::
    Parse(const std::shared_ptr<Buffer>& buffer,
          const std::shared_ptr<MediaTimer> playerTimer,
          RtpPacketsPlayerCallback* callback,
          const std::weak_ptr<BufferAllocator>& allocator)
{
    std::unique_ptr<RtpPacketsPlayerMediaFragment> fragment;
    if (buffer && callback && playerTimer) {
        auto deserializer = std::make_unique<WebMDeserializer>(allocator);
        const auto result = deserializer->Add(buffer);
        if (MaybeOk(result)) {
            if (deserializer->GetTracksCount()) {
                auto queue = std::make_shared<TasksQueue>(playerTimer,
                                                          std::move(deserializer),
                                                          callback);
                queue->SetTimerId(playerTimer->Register(queue));
                fragment.reset(new RtpPacketsPlayerMediaFragment(std::move(queue)));
            }
            else {
                MS_ERROR_STD("deserialized media buffer has no supported media tracks");
            }
        }
        else {
            MS_ERROR_STD("media buffer deserialization was failed: %s", ToString(result));
        }
    }
    return fragment;
}

RtpPacketsPlayerMediaFragment::TasksQueue::TasksQueue(const std::weak_ptr<MediaTimer> timerRef,
                                                      std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                      RtpPacketsPlayerCallback* callback)
    : _timerRef(timerRef)
    , _deserializer(std::move(deserializer))
    , _callback(callback)
{
}

RtpPacketsPlayerMediaFragment::TasksQueue::~TasksQueue()
{
    SetTimerId(0UL);
}

void RtpPacketsPlayerMediaFragment::TasksQueue::SetTimerId(uint64_t timerId)
{
    const auto oldTimerId = _timerId.exchange(timerId);
    if (timerId != oldTimerId && !timerId) {
        if (const auto timer = _timerRef.lock()) {
            timer->Unregister(oldTimerId);
        }
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        ClearTasks();
    }
}

void RtpPacketsPlayerMediaFragment::TasksQueue::Start(size_t trackIndex, uint32_t ssrc,
                                                      uint32_t clockRate,
                                                      uint64_t mediaId,
                                                      uint64_t mediaSourceId)
{
    if (GetTimerId()) {
        if (const auto mime = GetTrackType(trackIndex)) {
            SetClockRate(trackIndex, clockRate);
            auto startTask = std::make_unique<StartTask>(trackIndex, mime.value(),
                                                         ssrc, clockRate,
                                                         mediaId, mediaSourceId);
            Enque(std::move(startTask));
        }
    }
}

size_t RtpPacketsPlayerMediaFragment::TasksQueue::GetTracksCount() const
{
    LOCK_READ_PROTECTED_OBJ(_deserializer);
    return _deserializer->get()->GetTracksCount();
}

std::optional<RtpCodecMimeType> RtpPacketsPlayerMediaFragment::TasksQueue::
    GetTrackType(size_t trackIndex) const
{
    LOCK_READ_PROTECTED_OBJ(_deserializer);
    return _deserializer->get()->GetTrackType(trackIndex);
}

void RtpPacketsPlayerMediaFragment::TasksQueue::OnEvent(uint64_t /*timerId*/)
{
    std::unique_ptr<Task> task;
    {
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        if (!_tasks->empty()) {
            task = std::move(_tasks->front());
            _tasks->pop();
        }
    }
    Process(std::move(task));
}

void RtpPacketsPlayerMediaFragment::TasksQueue::SetClockRate(size_t trackIndex, uint32_t clockRate)
{
    LOCK_WRITE_PROTECTED_OBJ(_deserializer);
    _deserializer->get()->SetClockRate(trackIndex, clockRate);
}

void RtpPacketsPlayerMediaFragment::TasksQueue::Enque(std::unique_ptr<Task> task)
{
    if (task) {
        const auto type = task->GetType();
        {
            LOCK_WRITE_PROTECTED_OBJ(_tasks);
            if (TaskType::Start == type) {
                ClearTasks();
                LOCK_WRITE_PROTECTED_OBJ(_startTask);
                _startTask->reset();
            }
            _tasks->push(std::move(task));
        }
        if (TaskType::Start == type) {
            if (const auto timerId = GetTimerId()) {
                if (const auto timer = _timerRef.lock()) {
                    timer->SetTimeout(timerId, 0U);
                    timer->Start(timerId);
                }
            }
        }
    }
}

void RtpPacketsPlayerMediaFragment::TasksQueue::Process(std::unique_ptr<Task> task)
{
    if (task) {
        bool ok = false;
        if (TaskType::Start == task->GetType()) {
            std::unique_ptr<StartTask> startTask(static_cast<StartTask*>(task.release()));
            _callback->OnPlayStarted(startTask->GetMediaId(), startTask->GetMediaSourceId(),
                                     startTask->GetSsrc());
            // send 1st frame immediatelly and enque next if any
            ok = ReadNextFrame(startTask.get(), false) && ReadNextFrame(startTask.get(), true);
            if (ok) {
                LOCK_WRITE_PROTECTED_OBJ(_startTask);
                _startTask = std::move(startTask);
            }
        }
        else { // frame task
            LOCK_READ_PROTECTED_OBJ(_startTask);
            if (const auto& startTask = _startTask.ConstRef()) {
                Process(startTask.get(), task->TakePacket());
                ok = ReadNextFrame(startTask.get(), true);
            }
        }
        if (!ok) {
            if (const auto timerId = GetTimerId()) {
                if (const auto timer = _timerRef.lock()) {
                    timer->Stop(timerId);
                }
            }
        }
    }
}

void RtpPacketsPlayerMediaFragment::TasksQueue::Process(StartTask* startTask,
                                                        std::optional<RtpTranslatedPacket> packet)
{
    if (startTask && packet) {
        Timestamp timestamp = packet->GetTimestampOffset();
        packet->SetSsrc(startTask->GetSsrc());
        _callback->OnPlay(startTask->GetMediaId(), startTask->GetMediaSourceId(),
                          std::move(packet.value()));
        std::optional<uint32_t> timeout;
        if (!startTask->GeTimestamp().IsZero()) {
            const auto diff = timestamp - startTask->GeTimestamp();
            timeout = diff.ms<uint32_t>();
        }
        else {
            switch (startTask->GetMime().GetSubtype()) {
                case RtpCodecMimeType::Subtype::OPUS:
                    timeout = 20U; // 20 ms
                    break;
                default:
                    break;
            }
        }
        if (timeout.has_value()) {
            if (const auto timer = _timerRef.lock()) {
                timer->SetTimeout(GetTimerId(), timeout.value());
            }
        }
        startTask->SetTimestamp(std::move(timestamp));
    }
}

bool RtpPacketsPlayerMediaFragment::TasksQueue::ReadNextFrame(StartTask* startTask, bool enque)
{
    bool ok = false;
    if (startTask) {
        const auto trackIndex = startTask->GetTrackIndex();
        MediaFrameDeserializeResult result = MediaFrameDeserializeResult::Success;
        LOCK_WRITE_PROTECTED_OBJ(_deserializer);
        if (auto packet = _deserializer->get()->NextPacket(trackIndex)) {
            if (enque) {
                Enque(std::make_unique<MediaFrameTask>(std::move(packet)));
            }
            else {
                Process(startTask, std::move(packet));
            }
            ok = true;
        }
        else {
            result = _deserializer->get()->GetTrackLastResult(trackIndex);
            _callback->OnPlayFinished(startTask->GetMediaId(), startTask->GetMediaSourceId(),
                                      startTask->GetSsrc());
            _deserializer->get()->Clear();
        }
        if (!MaybeOk(result)) {
            MS_ERROR_STD("read of deserialized frames was failed: %s", ToString(result));
        }
    }
    return ok;
}

void RtpPacketsPlayerMediaFragment::TasksQueue::ClearTasks()
{
    while (!_tasks->empty()) {
        _tasks->pop();
    }
}

} // namespace RTC

namespace {

Task::Task(TaskType type)
    : _type(type)
{
}

StartTask::StartTask(size_t trackIndex, const RtpCodecMimeType& mime,
                     uint32_t ssrc, uint32_t clockRate,
                     uint64_t mediaId, uint64_t mediaSourceId)
    : Task(TaskType::Start)
    , _trackIndex(trackIndex)
    , _mime(mime)
    , _ssrc(ssrc)
    , _mediaId(mediaId)
    , _mediaSourceId(mediaSourceId)
    , _timestamp(clockRate)
{
}

MediaFrameTask::MediaFrameTask(std::optional<RtpTranslatedPacket> packet)
    : Task(TaskType::Frame)
    , _packet(std::move(packet))
{
}

}
