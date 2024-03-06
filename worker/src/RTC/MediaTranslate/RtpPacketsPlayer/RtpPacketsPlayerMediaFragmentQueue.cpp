#define MS_CLASS "RTC::RtpPacketsPlayerMediaFragmentQueue"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragmentQueue.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "Logger.hpp"

namespace {

enum class TaskType {
    Start,
    Frame
};

}

namespace RTC
{

class RtpPacketsPlayerMediaFragmentQueue::Task
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

class RtpPacketsPlayerMediaFragmentQueue::StartTask : public Task
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

class RtpPacketsPlayerMediaFragmentQueue::MediaFrameTask : public Task
{
public:
    MediaFrameTask(std::optional<RtpTranslatedPacket> packet);
    // override of Task
    std::optional<RtpTranslatedPacket> TakePacket() final { return std::move(_packet); }
private:
    std::optional<RtpTranslatedPacket> _packet;
};

RtpPacketsPlayerMediaFragmentQueue::
    RtpPacketsPlayerMediaFragmentQueue(const std::weak_ptr<MediaTimer>& timerRef,
                                       std::unique_ptr<MediaFrameDeserializer> deserializer,
                                       RtpPacketsPlayerCallback* callback)
    : _timerRef(timerRef)
    , _deserializer(std::move(deserializer))
    , _callback(callback)
{
}

RtpPacketsPlayerMediaFragmentQueue::~RtpPacketsPlayerMediaFragmentQueue()
{
    Stop();
}

std::shared_ptr<RtpPacketsPlayerMediaFragmentQueue> RtpPacketsPlayerMediaFragmentQueue::
    Create(const std::weak_ptr<MediaTimer>& timerRef,
           std::unique_ptr<MediaFrameDeserializer> deserializer,
           RtpPacketsPlayerCallback* callback)
{
    std::shared_ptr<RtpPacketsPlayerMediaFragmentQueue> queue;
    if (!timerRef.expired() && deserializer && callback) {
        queue.reset(new RtpPacketsPlayerMediaFragmentQueue(timerRef,
                                                           std::move(deserializer),
                                                           callback));
    }
    return queue;
}

void RtpPacketsPlayerMediaFragmentQueue::SetTimerId(uint64_t desiredTimerId, uint64_t expectedTimerId)
{
    if (desiredTimerId != expectedTimerId) {
        if (_timerId.compare_exchange_strong(expectedTimerId, desiredTimerId) && !desiredTimerId) {
            if (const auto timer = _timerRef.lock()) {
                timer->Unregister(expectedTimerId);
            }
            LOCK_WRITE_PROTECTED_OBJ(_tasks);
            if (ClearTasks()) {
                LOCK_WRITE_PROTECTED_OBJ(_startTask);
                if (const auto startTask = _startTask.Take()) {
                    _callback->OnPlayFinished(startTask->GetMediaId(),
                                              startTask->GetMediaSourceId(),
                                              startTask->GetSsrc());
                }
            }
        }
    }
}

void RtpPacketsPlayerMediaFragmentQueue::Start(size_t trackIndex, uint32_t ssrc,
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

void RtpPacketsPlayerMediaFragmentQueue::Stop()
{
    SetTimerId(0UL, GetTimerId());
    _framesTimeout = 0U;
}

size_t RtpPacketsPlayerMediaFragmentQueue::GetTracksCount() const
{
    LOCK_READ_PROTECTED_OBJ(_deserializer);
    return _deserializer->get()->GetTracksCount();
}

std::optional<RtpCodecMimeType> RtpPacketsPlayerMediaFragmentQueue::
    GetTrackType(size_t trackIndex) const
{
    LOCK_READ_PROTECTED_OBJ(_deserializer);
    return _deserializer->get()->GetTrackType(trackIndex);
}

void RtpPacketsPlayerMediaFragmentQueue::OnCallbackRegistered(uint64_t timerId, bool registered)
{
    if (registered) {
        SetTimerId(timerId, 0UL);
    }
    else {
        SetTimerId(0UL, timerId);
    }
}

void RtpPacketsPlayerMediaFragmentQueue::OnEvent(uint64_t /*timerId*/)
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

void RtpPacketsPlayerMediaFragmentQueue::SetClockRate(size_t trackIndex, uint32_t clockRate)
{
    LOCK_WRITE_PROTECTED_OBJ(_deserializer);
    _deserializer->get()->SetClockRate(trackIndex, clockRate);
}

void RtpPacketsPlayerMediaFragmentQueue::Enque(std::unique_ptr<Task> task)
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

void RtpPacketsPlayerMediaFragmentQueue::Process(std::unique_ptr<Task> task)
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

void RtpPacketsPlayerMediaFragmentQueue::Process(StartTask* startTask,
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
        if (timeout.has_value() && timeout.value() != _framesTimeout.exchange(timeout.value())) {
            if (const auto timer = _timerRef.lock()) {
                timer->SetTimeout(GetTimerId(), timeout.value());
            }
        }
        startTask->SetTimestamp(std::move(timestamp));
    }
}

bool RtpPacketsPlayerMediaFragmentQueue::ReadNextFrame(StartTask* startTask, bool enque)
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

bool RtpPacketsPlayerMediaFragmentQueue::ClearTasks()
{
    if (!_tasks->empty()) {
        while (!_tasks->empty()) {
            _tasks->pop();
        }
        return true;
    }
    return false;
}

RtpPacketsPlayerMediaFragmentQueue::Task::Task(TaskType type)
    : _type(type)
{
}

RtpPacketsPlayerMediaFragmentQueue::StartTask::StartTask(size_t trackIndex,
                                                         const RtpCodecMimeType& mime,
                                                         uint32_t ssrc, uint32_t clockRate,
                                                         uint64_t mediaId,
                                                         uint64_t mediaSourceId)
    : Task(TaskType::Start)
    , _trackIndex(trackIndex)
    , _mime(mime)
    , _ssrc(ssrc)
    , _mediaId(mediaId)
    , _mediaSourceId(mediaSourceId)
    , _timestamp(clockRate)
{
}

RtpPacketsPlayerMediaFragmentQueue::MediaFrameTask::
    MediaFrameTask(std::optional<RtpTranslatedPacket> packet)
    : Task(TaskType::Frame)
    , _packet(std::move(packet))
{
}

} // namespace RTC
