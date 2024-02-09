#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "absl/functional/any_invocable.h"
#include "ProtectedObj.hpp"
#include "DepLibUV.hpp"
#include "api/units/time_delta.h"
#include <queue>
#include <map>
#include <sched.h>
#include <time.h>

// see https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/rtc_base/task_queue_stdlib.cc

namespace {

template<typename T>
class ThreadObject
{
public:
    T* operator&() { return &_object; }
protected:
    T _object;
};

class ThreadAttributes : public ThreadObject<pthread_attr_t>
{
public:
    ThreadAttributes();
    ~ThreadAttributes();
};

class ThreadMutex : public ThreadObject<pthread_mutex_t>
{
public:
    ThreadMutex();
    ~ThreadMutex();
    void Lock() { pthread_mutex_lock(&_object); }
    void Unlock() { pthread_mutex_unlock(&_object); }
    // for std compatibility
    void lock() { Lock(); }
    void unlock() { Unlock(); }
};

class ThreadCondition : public ThreadObject<pthread_cond_t>
{
public:
    ThreadCondition();
    ~ThreadCondition();
    void Signal() { pthread_cond_signal(&_object); }
    int TimedWait(ThreadMutex& mutex, const struct timespec* absTime);
};

using OrderId = uint64_t;

struct DelayedEntryTimeout {
    uint64_t _nextFireAtMs = 0ULL;
    OrderId _order = 0ULL;
    bool operator<(const DelayedEntryTimeout& o) const {
        return std::tie(_nextFireAtMs, _order) < std::tie   (o._nextFireAtMs, o._order);
    }
};

struct NextTask {
    bool _finalTask = false;
    absl::AnyInvocable<void()> _runTask;
    webrtc::TimeDelta _sleepTime = webrtc::TimeDelta::PlusInfinity();
  };

class TaskManager
{
public:
    virtual ~TaskManager() = default;
    virtual uint64_t StartTask(absl::AnyInvocable<void()> task, uint64_t delayMs, bool once) = 0;
    virtual void StopTask(uint64_t taskId) = 0;
};

class TimerQueue : public TaskManager
{
public:
    TimerQueue();
    TimerQueue(const TimerQueue&) = delete;
    TimerQueue(TimerQueue&&) = delete;
    TimerQueue& operator = (const TimerQueue&) = delete;
    TimerQueue& operator = (TimerQueue&&) = delete;
    ~TimerQueue() final;
    int StartThread(void* (*func)(void*), void* arg);
    bool FireExpiredTimers();
    // impl. of TaskManager
    uint64_t StartTask(absl::AnyInvocable<void()> task, uint64_t delayMs, bool once) final;
    void StopTask(uint64_t taskId) final;
private:
    NextTask GetNextTask();
    void ProcessTasks();
    void NotifyWake();
private:
    pthread_t _thread = 0;
    ThreadAttributes _attr;
    //ThreadMutex _mutex;
    //ThreadCondition _cond;
    //ThreadMutex _cond_mutex;
    struct sched_param _param;
    
    // Signaled whenever a new task is pending.
    ThreadCondition _flagNotify;
    
    ThreadMutex _pendingLock;
    // Holds the next order to use for the next task to be
    // put into one of the pending queues.
    OrderId _threadPostingOrder = 0ULL;
    // The list of all pending tasks that need to be processed in the
      // FIFO queue ordering on the worker thread.
    std::deque<std::pair<OrderId, absl::AnyInvocable<void()>>> _pendingQueue;
    // The list of all pending tasks that need to be processed at a future
    // time based upon a delay. On the off change the delayed task should
    // happen at exactly the same time interval as another task then the
    // task is processed based on FIFO ordering. std::priority_queue was
    // considered but rejected due to its inability to extract the
    // move-only value out of the queue without the presence of a hack.
    std::map<DelayedEntryTimeout, absl::AnyInvocable<void()>> _delayedQueue;
};

}

namespace RTC
{

class MediaTimerHandleUnix : public MediaTimerHandle
{
public:
    MediaTimerHandleUnix(const std::weak_ptr<MediaTimerCallback>& callbackRef,
                         const std::weak_ptr<TaskManager>& managerRef);
    ~MediaTimerHandleUnix() final;
    // impl. of MediaTimerHandle
    void Start(bool singleshot) final;
    void Stop() final;
private:
    const std::weak_ptr<TaskManager> _managerRef;
    ProtectedObj<uint64_t> _taskId = 0ULL;
};

class MediaTimerHandleFactoryUnix : public MediaTimerHandleFactory
{
public:
    MediaTimerHandleFactoryUnix(const std::string& timerName);
    ~MediaTimerHandleFactoryUnix() final;
    void SetTimerQueue(std::shared_ptr<TimerQueue> queue);
    static void* ThreadFunc(void* param);
    // impl. of MediaTimerHandleFactory
    std::unique_ptr<MediaTimerHandle> CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef) final;
private:
    void Run();
    bool IsCancelled() const { return _cancelled.load(); }
    void SetCancelled();
    static void SetThreadName(const std::string& timerName);
private:
    const std::string _timerName;
    std::shared_ptr<TimerQueue> _queue;
    std::atomic_bool _cancelled = false;
};


MediaTimerHandleUnix::MediaTimerHandleUnix(const std::weak_ptr<MediaTimerCallback>& callbackRef,
                                           const std::weak_ptr<TaskManager>& managerRef)
    : MediaTimerHandle(callbackRef)
    , _managerRef(managerRef)
{
}

MediaTimerHandleUnix::~MediaTimerHandleUnix()
{
    MediaTimerHandleUnix::Stop();
}

void MediaTimerHandleUnix::Start(bool singleshot)
{
    if (IsCallbackValid()) {
        if (const auto manager = _managerRef.lock()) {
            LOCK_WRITE_PROTECTED_OBJ(_taskId);
            if (!_taskId.ConstRef()) {
                _taskId = manager->StartTask([callbackRef = GetCallbackRef()]() {
                    if (const auto callback = callbackRef.lock()) {
                        callback->OnEvent();
                    }
                }, GetTimeout(), singleshot);
            }
        }
    }
}

void MediaTimerHandleUnix::Stop()
{
    if (const auto manager = _managerRef.lock()) {
        LOCK_WRITE_PROTECTED_OBJ(_taskId);
        if (const auto taskId = _taskId.Exchange(0ULL)) {
            manager->StopTask(taskId);
        }
    }
}

MediaTimerHandleFactoryUnix::MediaTimerHandleFactoryUnix(const std::string& timerName)
    : _timerName(timerName)
{
}

MediaTimerHandleFactoryUnix::~MediaTimerHandleFactoryUnix()
{
    SetTimerQueue(nullptr);
}

void MediaTimerHandleFactoryUnix::SetTimerQueue(std::shared_ptr<TimerQueue> queue)
{
    std::atomic_store(&_queue, std::move(queue));
}

void* MediaTimerHandleFactoryUnix::ThreadFunc(void* param)
{
    static_cast<MediaTimerHandleFactoryUnix*>(param)->Run();
    return 0;
}

std::unique_ptr<MediaTimerHandle> MediaTimerHandleFactoryUnix::
    CreateHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    if (!IsCancelled()) {
        if (const auto queue = std::atomic_load(&_queue)) {
            return std::make_unique<MediaTimerHandleUnix>(callbackRef, queue);
        }
    }
    return nullptr;
}

void MediaTimerHandleFactoryUnix::Run()
{
    SetThreadName(_timerName);
    while (!IsCancelled()) {
        const auto queue = std::atomic_load(&_queue);
        if (!queue || !queue->FireExpiredTimers()) {
            break;
        }
    }
}

void MediaTimerHandleFactoryUnix::SetCancelled()
{
    if (!_cancelled.exchange(true)) {
        
    }
}

void MediaTimerHandleFactoryUnix::SetThreadName(const std::string& timerName)
{
    if (!timerName.empty()) {
#ifdef __APPLE__
        pthread_setname_np(timerName.c_str());
#else
        prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(timerName.c_str()));
#endif
    }
}

std::unique_ptr<MediaTimerHandleFactory> MediaTimerHandleFactory::Create(const std::string& timerName)
{
    auto factory = std::make_unique<MediaTimerHandleFactoryUnix>(timerName);
    auto queue = std::make_shared<TimerQueue>();
    if (0 != queue->StartThread(&MediaTimerHandleFactoryUnix::ThreadFunc, factory.get())) {
        factory.reset();
    }
    else {
        factory->SetTimerQueue(std::move(queue));
    }
    return factory;
}

} // namespace RTC

namespace {

/*inline void TimespecGetTimeOfDay(struct timespec* tspec)
{
    timespec_get(tspec, TIME_UTC);
}*/

ThreadAttributes::ThreadAttributes()
{
    pthread_attr_init(&_object);
}

ThreadAttributes::~ThreadAttributes()
{
    pthread_attr_destroy(&_object);
}

ThreadMutex::ThreadMutex()
{
    pthread_mutex_init(&_object, nullptr);
}

ThreadMutex::~ThreadMutex()
{
    pthread_mutex_destroy(&_object);
}

ThreadCondition::ThreadCondition()
{
    pthread_cond_init(&_object, nullptr);
}

ThreadCondition::~ThreadCondition()
{
    pthread_cond_destroy(&_object);
}

int ThreadCondition::TimedWait(ThreadMutex& mutex, const struct timespec* absTime)
{
    return pthread_cond_timedwait(&_object, &mutex, absTime);
}

TimerQueue::TimerQueue()
{
    pthread_attr_setstacksize(&_attr, 1024U * 1024U);
    pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_JOINABLE);
    _param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&_attr, &_param);
    pthread_attr_setschedpolicy(&_attr, SCHED_FIFO);
}

TimerQueue::~TimerQueue()
{
    if (_thread) {
        pthread_cancel(_thread);
        pthread_join(_thread, nullptr);
    }
}

int TimerQueue::StartThread(void* (*func)(void*), void* arg)
{
    if (0 == _thread) {
        return pthread_create(&_thread, &_attr, func, arg);
    }
    return 0; // already started
}

bool TimerQueue::FireExpiredTimers()
{
    //struct timespec timeout;
    //TimespecGetTimeOfDay(&timeout);
    //_cond_mutex.Lock();
    return true;
}

uint64_t TimerQueue::StartTask(absl::AnyInvocable<void()> task, uint64_t delayMs, bool once)
{
    uint64_t result = 0ULL;
    if (task) {
        {
            const std::unique_lock<ThreadMutex> lock(_pendingLock);
            const auto order = ++_threadPostingOrder;
            if (0UL == delayMs) {
                _pendingQueue.push_back(std::make_pair(order, std::move(task)));
            }
            else {
                DelayedEntryTimeout delayedEntry;
                delayedEntry._nextFireAtMs = DepLibUV::GetTimeMs() + delayMs;
                delayedEntry._order = order;
                _delayedQueue[delayedEntry] = std::move(task);
            }
            result = static_cast<uint64_t>(order);
        }
        NotifyWake();
    }
    return result;
}

void TimerQueue::StopTask(uint64_t taskId)
{
    if (taskId) {
        const std::unique_lock<ThreadMutex> lock(_pendingLock);
        bool foundPending = false;
        for (auto it = _pendingQueue.begin(); it != _pendingQueue.end(); ++it) {
            if (it->first == taskId) {
                foundPending = true;
                _pendingQueue.erase(it);
                break;
            }
        }
        if (!foundPending) {
            for (auto it = _delayedQueue.begin(); it != _delayedQueue.end(); ++it) {
                if (it->first._order == taskId) {
                    _delayedQueue.erase(it);
                    break;
                }
            }
        }
    }
}

NextTask TimerQueue::GetNextTask()
{
    
}

void TimerQueue::ProcessTasks()
{
    
}

void TimerQueue::NotifyWake()
{
    
}

}
