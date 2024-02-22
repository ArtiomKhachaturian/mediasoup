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
#include "RTC/MediaTranslate/Buffers/Buffer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Timestamp.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include "absl/container/flat_hash_map.h"
#include <atomic>
#include <queue>

namespace {

using namespace RTC;

enum class TaskType {
    Start,
    Finish,
    Frame
};

class Task
{
public:
    virtual ~Task() = default;
    TaskType GetType() const { return _type; }
    bool IsStartTask() const { return TaskType::Start == GetType(); }
    bool IsFinishTask() const { return TaskType::Finish == GetType(); }
    virtual std::shared_ptr<const MediaFrame> GetFrame() const { return nullptr; }
    virtual size_t GetTrackIndex() const { return 0U; }
protected:
    Task(TaskType type);
private:
    const TaskType _type;
};

class StartFinishTask : public Task
{
public:
    StartFinishTask(bool start);
};

class MediaFrameTask : public Task
{
public:
    MediaFrameTask(std::shared_ptr<const MediaFrame> frame, size_t trackIndex);
    // override of Task
    std::shared_ptr<const MediaFrame> GetFrame() const final { return _frame; }
    size_t GetTrackIndex() const final { return _trackIndex; }
private:
    const std::shared_ptr<const MediaFrame> _frame;
    const size_t _trackIndex;
};

// key is track number, value - packetizer instance
using Packetizers = absl::flat_hash_map<size_t, std::unique_ptr<RTC::RtpPacketizer>>;

}

namespace RTC
{

class RtpPacketsPlayerMediaFragment::TasksQueue : public MediaTimerCallback
{
public:
    TasksQueue(const std::weak_ptr<MediaTimer> timerRef, uint32_t ssrc, uint32_t clockRate,
               uint8_t payloadType, uint64_t mediaId, uint64_t mediaSourceId,
               std::unique_ptr<MediaFrameDeserializer> deserializer,
               Packetizers packetizers, RtpPacketsPlayerCallback* callback);
    ~TasksQueue() final;
    void SetTimerId(uint64_t timerId);
    void Start();
    // impl. of MediaTimerCallback
    void OnEvent(uint64_t timerId) final;
private:
    uint64_t GetTimerId() const { return _timerId.load(); }
    void EnqueTask(std::unique_ptr<Task> task);
    void ProcessTask(std::unique_ptr<Task> task, bool nextIsFinishTask);
    bool ProcessFrame(const std::shared_ptr<const MediaFrame>& frame,
                      size_t trackIndex, bool nextIsFinishTask);
private:
    const std::weak_ptr<MediaTimer> _timerRef;
    const uint32_t _ssrc;
    const uint8_t _payloadType;
    const uint64_t _mediaId;
    const uint64_t _mediaSourceId;
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    const Packetizers _packetizers;
    RtpPacketsPlayerCallback* const _callback;
    std::atomic<uint64_t> _timerId = 0;
    // used for adjust of timer interval
    Timestamp _previous; // TODO: it should be separate for each track, because maybe diffrent clock rate on tracks
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

void RtpPacketsPlayerMediaFragment::Start()
{
    _queue->Start();
}

std::shared_ptr<RtpPacketsPlayerMediaFragment> RtpPacketsPlayerMediaFragment::
    Parse(const RtpCodecMimeType& mime, const std::shared_ptr<Buffer>& buffer,
          const std::shared_ptr<MediaTimer> playerTimer,
          uint32_t ssrc, uint32_t clockRate,
          uint8_t payloadType, uint64_t mediaId, uint64_t mediaSourceId,
          RtpPacketsPlayerCallback* callback,
          const std::weak_ptr<BufferAllocator>& allocator)
{
    std::shared_ptr<RtpPacketsPlayerMediaFragment> fragment;
    if (buffer && callback && playerTimer) {
        if (WebMCodecs::IsSupported(mime)) {
            auto deserializer = std::make_unique<WebMDeserializer>(allocator);
            const auto result = deserializer->Add(buffer);
            if (MaybeOk(result)) {
                if (const auto tracksCount = deserializer->GetTracksCount()) {
                    size_t acceptedTracksCount = 0UL;
                    Packetizers packetizers;
                    for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                        const auto trackMime = deserializer->GetTrackMimeType(trackIndex);
                        if (trackMime.has_value() && trackMime.value() == mime) {
                            std::unique_ptr<RtpPacketizer> packetizer;
                            switch (mime.GetSubtype()) {
                                case RtpCodecMimeType::Subtype::OPUS:
                                    packetizer = std::make_unique<RtpPacketizerOpus>();
                                    break;
                                default:
                                    break;
                            }
                            if (packetizer) {
                                deserializer->SetClockRate(trackIndex, clockRate);
                                packetizers[trackIndex] = std::move(packetizer);
                                ++acceptedTracksCount;
                            }
                            else {
                                MS_ERROR_STD("packetizer for [%s] not yet implemented", mime.ToString().c_str());
                            }
                        }
                    }
                    if (acceptedTracksCount > 0U) {
                        auto queue = std::make_shared<TasksQueue>(playerTimer,
                                                                  ssrc,
                                                                  clockRate,
                                                                  payloadType,
                                                                  mediaId,
                                                                  mediaSourceId,
                                                                  std::move(deserializer),
                                                                  std::move(packetizers),
                                                                  callback);
                        queue->SetTimerId(playerTimer->Register(queue));
                        fragment.reset(new RtpPacketsPlayerMediaFragment(std::move(queue)));
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
        else {
            MS_ERROR("WebM unsupported MIME type %s", GetStreamInfoString(mime, ssrc).c_str());
        }
    }
    return fragment;
}

RtpPacketsPlayerMediaFragment::TasksQueue::TasksQueue(const std::weak_ptr<MediaTimer> timerRef,
                                                      uint32_t ssrc, uint32_t clockRate,
                                                      uint8_t payloadType, uint64_t mediaId,
                                                      uint64_t mediaSourceId,
                                                      std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                      Packetizers packetizers, RtpPacketsPlayerCallback* callback)
    : _timerRef(timerRef)
    , _ssrc(ssrc)
    , _payloadType(payloadType)
    , _mediaId(mediaId)
    , _mediaSourceId(mediaSourceId)
    , _deserializer(std::move(deserializer))
    , _packetizers(std::move(packetizers))
    , _callback(callback)
    , _previous(clockRate)
{
}

RtpPacketsPlayerMediaFragment::TasksQueue::~TasksQueue()
{
    SetTimerId(0UL);
    LOCK_WRITE_PROTECTED_OBJ(_tasks);
    while (!_tasks->empty()) {
        _tasks->pop();
    }
}

void RtpPacketsPlayerMediaFragment::TasksQueue::SetTimerId(uint64_t timerId)
{
    const auto oldTimerId = _timerId.exchange(timerId);
    if (timerId != oldTimerId && !timerId) {
        if (const auto timer = _timerRef.lock()) {
            timer->Unregister(oldTimerId);
        }
    }
}

void RtpPacketsPlayerMediaFragment::TasksQueue::Start()
{
    MediaFrameDeserializeResult result = MediaFrameDeserializeResult::Success;
    bool started = false;
    for (auto it = _packetizers.begin(); it != _packetizers.end(); ++it) {
        while (auto frame = _deserializer->ReadNextFrame(it->first, &result)) {
            if (!started) {
                EnqueTask(std::make_unique<StartFinishTask>(true));
                started = true;
            }
            EnqueTask(std::make_unique<MediaFrameTask>(std::move(frame), it->first));
        }
    }
    if (started) {
        EnqueTask(std::make_unique<StartFinishTask>(false));
    }
    if (!MaybeOk(result)) {
        MS_ERROR_STD("read of deserialized frames was failed: %s", ToString(result));
    }
}

void RtpPacketsPlayerMediaFragment::TasksQueue::OnEvent(uint64_t /*timerId*/)
{
    std::unique_ptr<Task> task;
    bool nextIsFinishTask = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        if (!_tasks->empty()) {
            task = std::move(_tasks->front());
            _tasks->pop();
            if (_tasks->empty()) {
                if (const auto timer = _timerRef.lock()) {
                    timer->Stop(GetTimerId());
                }
            }
            else {
                nextIsFinishTask = _tasks->front()->IsFinishTask();
            }
        }
    }
    ProcessTask(std::move(task), nextIsFinishTask);
}

void RtpPacketsPlayerMediaFragment::TasksQueue::EnqueTask(std::unique_ptr<Task> task)
{
    if (task) {
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        _tasks->push(std::move(task));
        if (1U == _tasks->size()) {
            if (const auto timerId = GetTimerId()) {
                if (const auto timer = _timerRef.lock()) {
                    timer->SetTimeout(timerId, 0U);
                    timer->Start(timerId);
                }
            }
        }
    }
}

void RtpPacketsPlayerMediaFragment::TasksQueue::ProcessTask(std::unique_ptr<Task> task,
                                                            bool nextIsFinishTask)
{
    if (task) {
        switch (task->GetType()) {
            case TaskType::Start:
                _previous.SetTime(webrtc::Timestamp::Zero());
                _callback->OnPlayStarted(_ssrc, _mediaId, _mediaSourceId);
                break;
            case TaskType::Finish:
                _callback->OnPlayFinished(_ssrc, _mediaId, _mediaSourceId);
                break;
            case TaskType::Frame:
                ProcessFrame(task->GetFrame(), task->GetTrackIndex(), nextIsFinishTask);
                break;
            default:
                MS_ASSERT(false, "unsupported task");
                break;
        }
    }
}

bool RtpPacketsPlayerMediaFragment::TasksQueue::
    ProcessFrame(const std::shared_ptr<const MediaFrame>& frame, size_t trackIndex,
                 bool nextIsFinishTask)
{
    if (frame) {
        const auto it = _packetizers.find(trackIndex);
        if (it != _packetizers.end()) {
            if (const auto packet = it->second->AddFrame(frame)) {
                const auto& timestamp = frame->GetTimestamp();
                packet->SetSsrc(_ssrc);
                packet->SetPayloadType(_payloadType);
                _callback->OnPlay(timestamp, packet, _mediaId, _mediaSourceId);
                if (!_previous.IsZero()) {
                    if (const auto timer = _timerRef.lock()) {
                        uint32_t timeout = 0U;
                        if (!nextIsFinishTask) {
                            const auto diff = timestamp - _previous;
                            timeout = diff.ms<uint32_t>();
                        }
                        timer->SetTimeout(GetTimerId(), timeout);
                    }
                }
                _previous = timestamp;
                return true;
            }
        }
    }
    return false;
}

} // namespace RTC

namespace {

Task::Task(TaskType type)
    : _type(type)
{
}

StartFinishTask::StartFinishTask(bool start)
    : Task(start ? TaskType::Start : TaskType::Finish)
{
}

MediaFrameTask::MediaFrameTask(std::shared_ptr<const MediaFrame> frame, size_t trackIndex)
    : Task(TaskType::Frame)
    , _frame(std::move(frame))
    , _trackIndex(trackIndex)
{
}

}
