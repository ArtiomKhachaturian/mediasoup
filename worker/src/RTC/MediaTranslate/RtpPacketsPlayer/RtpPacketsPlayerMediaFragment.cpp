#define MS_CLASS "RTC::RtpPacketsPlayerMediaFragment"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/Buffer.hpp"
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
    virtual std::shared_ptr<MediaFrame> TakeFrame() { return nullptr; }
protected:
    Task(TaskType type);
private:
    const TaskType _type;
};

class StartTask : public Task
{
public:
    StartTask(size_t trackIndex, uint32_t ssrc,
              uint32_t clockRate, uint8_t payloadType,
              uint64_t mediaId, uint64_t mediaSourceId,
              const std::shared_ptr<RtpPacketizer>& packetizer);
    size_t GetTrackIndex() const { return _trackIndex; }
    uint32_t GetSsrc() const { return _ssrc; }
    const uint8_t GetPayloadType() const { return _payloadType; }
    const uint64_t GetMediaId() const { return _mediaId; }
    const uint64_t GetMediaSourceId() const { return _mediaSourceId; }
    const std::shared_ptr<RtpPacketizer>& GetPacketizer() const { return _packetizer; }
    const Timestamp& GeTimestamp() const { return _timestamp; }
    void SetTimestamp(const Timestamp& timestamp) { _timestamp = timestamp; }
private:
    const size_t _trackIndex;
    const uint32_t _ssrc;
    const uint8_t _payloadType;
    const uint64_t _mediaId;
    const uint64_t _mediaSourceId;
    const std::shared_ptr<RtpPacketizer> _packetizer;
    Timestamp _timestamp;
};

class MediaFrameTask : public Task
{
public:
    MediaFrameTask(std::shared_ptr<MediaFrame> frame);
    // override of Task
    std::shared_ptr<MediaFrame> TakeFrame() final { return std::move(_frame); }
private:
    std::shared_ptr<MediaFrame> _frame;
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
    void Start(size_t trackIndex, uint32_t ssrc,
               uint32_t clockRate, uint8_t payloadType,
               uint64_t mediaId, uint64_t mediaSourceId,
               const std::shared_ptr<RtpPacketizer>& packetizer);
    // impl. of MediaTimerCallback
    void OnEvent(uint64_t timerId) final;
private:
    uint64_t GetTimerId() const { return _timerId.load(); }
    void SetClockRate(size_t trackIndex, uint32_t clockRate);
    void Enque(std::unique_ptr<Task> task);
    void Process(std::unique_ptr<Task> task);
    void Process(StartTask* startTask, const std::shared_ptr<MediaFrame>& frame);
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

RtpPacketsPlayerMediaFragment::RtpPacketsPlayerMediaFragment(Packetizers packetizers,
                                                             std::shared_ptr<TasksQueue> queue)
    : _packetizers(std::move(packetizers))
    , _queue(queue)
{
}

RtpPacketsPlayerMediaFragment::~RtpPacketsPlayerMediaFragment()
{
    _queue->SetTimerId(0ULL);
}

void RtpPacketsPlayerMediaFragment::Start(size_t trackIndex, uint32_t ssrc,
                                          uint32_t clockRate, uint8_t payloadType,
                                          uint64_t mediaId, uint64_t mediaSourceId)
{
    if (trackIndex < _packetizers.size()) {
        auto it = _packetizers.begin();
        std::advance(it, trackIndex);
        _queue->Start(it->first, ssrc, clockRate, payloadType, mediaId, mediaSourceId, it->second);
    }
}

size_t RtpPacketsPlayerMediaFragment::GetTracksCount() const
{
    return _packetizers.size();
}

std::optional<RtpCodecMimeType> RtpPacketsPlayerMediaFragment::GetTrackMimeType(size_t trackIndex) const
{
    if (trackIndex < _packetizers.size()) {
        auto it = _packetizers.begin();
        std::advance(it, trackIndex);
        return it->second->GetType();
    }
    return std::nullopt;
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
            if (const auto tracksCount = deserializer->GetTracksCount()) {
                size_t acceptedTracksCount = 0UL;
                Packetizers packetizers;
                for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                    const auto mime = deserializer->GetTrackMimeType(trackIndex);
                    if (mime.has_value()) {
                        if (WebMCodecs::IsSupported(mime.value())) {
                            std::shared_ptr<RtpPacketizer> packetizer;
                            switch (mime->GetSubtype()) {
                                case RtpCodecMimeType::Subtype::OPUS:
                                    packetizer = std::make_shared<RtpPacketizerOpus>();
                                    break;
                                default:
                                    break;
                            }
                            if (packetizer) {
                                packetizers[trackIndex] = std::move(packetizer);
                                ++acceptedTracksCount;
                            }
                            else {
                                MS_ERROR_STD("packetizer for [%s] not yet implemented", mime->ToString().c_str());
                            }
                        }
                        else {
                            MS_WARN_DEV_STD("WebM unsupported MIME type %s", mime->ToString().c_str());
                        }
                    }
                }
                if (acceptedTracksCount > 0U) {
                    auto queue = std::make_shared<TasksQueue>(playerTimer,
                                                              std::move(deserializer),
                                                              callback);
                    queue->SetTimerId(playerTimer->Register(queue));
                    fragment.reset(new RtpPacketsPlayerMediaFragment(std::move(packetizers),
                                                                     std::move(queue)));
                }
                else {
                    // TODO: log error
                }
            }
            else {
                MS_ERROR_STD("deserialized media buffer has no media tracks");
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
                                                      uint32_t clockRate, uint8_t payloadType,
                                                      uint64_t mediaId, uint64_t mediaSourceId,
                                                      const std::shared_ptr<RtpPacketizer>& packetizer)
{
    if (packetizer && GetTimerId()) {
        SetClockRate(trackIndex, clockRate);
        auto startTask = std::make_unique<StartTask>(trackIndex, ssrc, clockRate,
                                                     payloadType, mediaId,
                                                     mediaSourceId, packetizer);
        Enque(std::move(startTask));
    }
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
            _callback->OnPlayStarted(startTask->GetSsrc(),
                                     startTask->GetMediaId(),
                                     startTask->GetMediaSourceId());
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
                Process(startTask.get(), task->TakeFrame());
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
                                                        const std::shared_ptr<MediaFrame>& frame)
{
    if (startTask && frame) {
        if (const auto packet = startTask->GetPacketizer()->AddFrame(frame)) {
            const auto& timestamp = frame->GetTimestamp();
            packet->SetSsrc(startTask->GetSsrc());
            packet->SetPayloadType(startTask->GetPayloadType());
            _callback->OnPlay(timestamp, packet, startTask->GetMediaId(),
                              startTask->GetMediaSourceId());
            std::optional<uint32_t> timeout;
            if (!startTask->GeTimestamp().IsZero()) {
                const auto diff = timestamp - startTask->GeTimestamp();
                timeout = diff.ms<uint32_t>();
            }
            else {
                switch (frame->GetMimeType().GetSubtype()) {
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
            startTask->SetTimestamp(timestamp);
        }
    }
}

bool RtpPacketsPlayerMediaFragment::TasksQueue::ReadNextFrame(StartTask* startTask, bool enque)
{
    bool ok = false;
    if (startTask) {
        MediaFrameDeserializeResult result = MediaFrameDeserializeResult::  Success;
        LOCK_WRITE_PROTECTED_OBJ(_deserializer);
        if (auto frame = _deserializer->get()->ReadNextFrame(startTask->GetTrackIndex(),
                                                             &result)) {
            if (enque) {
                Enque(std::make_unique<MediaFrameTask>(std::move(frame)));
            }
            else {
                Process(startTask, frame);
            }
            ok = true;
        }
        else {
            _callback->OnPlayFinished(startTask->GetSsrc(),
                                      startTask->GetMediaId(),
                                      startTask->GetMediaSourceId());
            LOCK_WRITE_PROTECTED_OBJ(_deserializer);
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

StartTask::StartTask(size_t trackIndex, uint32_t ssrc,
                     uint32_t clockRate, uint8_t payloadType,
                     uint64_t mediaId, uint64_t mediaSourceId,
                     const std::shared_ptr<RtpPacketizer>& packetizer)
    : Task(TaskType::Start)
    , _trackIndex(trackIndex)
    , _ssrc(ssrc)
    , _payloadType(payloadType)
    , _mediaId(mediaId)
    , _mediaSourceId(mediaSourceId)
    , _packetizer(packetizer)
    , _timestamp(clockRate)
{
}

MediaFrameTask::MediaFrameTask(std::shared_ptr<MediaFrame> frame)
    : Task(TaskType::Frame)
    , _frame(std::move(frame))
{
}

}
