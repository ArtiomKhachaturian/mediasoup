#define MS_CLASS "RTC::MediaTimerHandleFactoryUV"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactoryUV.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/ThreadUtils.hpp"
#include "UVAsyncHandle.hpp"
#include "Logger.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>
#include <variant>
#include <queue>
#include <thread>

namespace {

using namespace RTC;

using UVLoop = UVHandle<uv_loop_t>;
using UVTimer = UVHandle<uv_timer_t>;

enum class TimerCommandType {
    Add,
    Remove,
    Start,
    Stop,
    SetTimeout
};

class TimerWrapper
{
public:
    TimerWrapper(const std::weak_ptr<MediaTimerCallback>& callbackRef,
                 uint64_t timerId, UVTimer timer);
    ~TimerWrapper();
    void SetTimeout(uint32_t timeoutMs);
    void Start(bool singleshot);
    void Stop();
    bool IsActive() const;
private:
    void Invoke();
    void OnFired();
private:
    const std::weak_ptr<MediaTimerCallback> _callbackRef;
    const uint64_t _timerId;
    const UVTimer _timer;
    uint32_t _timeoutMs = 0;
    bool _singleshot = false;
};

class TimerCommand
{
    using Data = std::variant<std::weak_ptr<MediaTimerCallback>, bool, uint32_t>;
public:
    TimerCommand() = delete;
    TimerCommand(uint64_t timerId, const std::weak_ptr<MediaTimerCallback>& callbackRef); // add
    TimerCommand(uint64_t timerId, bool start, bool singleshot); // start/stop
    TimerCommand(uint64_t timerId, uint32_t timeoutMs); // timeout
    TimerCommand(uint64_t timerId); // remove
    TimerCommand(TimerCommand&&) = default;
    TimerCommand(const TimerCommand&) = default;
    TimerCommandType GetType() const { return _type; }
    uint64_t GetTimerId() const { return _timerId; }
    std::weak_ptr<MediaTimerCallback> GetCallback() const noexcept(false);
    uint32_t GetTimeout() const noexcept(false);
    bool IsSingleshot() const noexcept(false);
public:
    TimerCommandType _type;
    uint64_t _timerId = 0ULL;
    Data _data;
};

class TimerCommandManager
{
public:
    virtual ~TimerCommandManager() = default;
    virtual void Add(TimerCommand command) = 0;
    virtual bool IsTimerStarted(uint64_t timerId) const = 0;
};

}

namespace RTC
{

class MediaTimerHandleUV : public MediaTimerHandle
{
public:
    MediaTimerHandleUV(const std::shared_ptr<MediaTimerCallback>& callback,
                       const std::shared_ptr<TimerCommandManager>& manager);
    ~MediaTimerHandleUV() final;
    // impl. of MediaTimerHandle
    void Start(bool singleshot) final;
    void Stop() final;
    bool IsStarted() const final;
protected:
    void OnTimeoutChanged(uint32_t timeoutMs) final;
private:
    uint64_t GetTimerId() const { return reinterpret_cast<uint64_t>(this); }
    void SetTimeout(uint32_t timeoutMs);
private:
    const std::weak_ptr<TimerCommandManager> _managerRef;
};

class MediaTimerHandleFactoryUV::Impl : public TimerCommandManager
{
    using CommandsQueue = std::queue<TimerCommand>;
public:
    Impl(UVLoop loop);
    ~Impl() final;
    int RunLoop();
    void StopLoop();
    // impl. of TimerCommandManager
    void Add(TimerCommand command) final;
    bool IsTimerStarted(uint64_t timerId) const final;
private:
    static void OnCommand(uv_async_t* handle);
    static void OnStop(uv_async_t* handle);
    bool HasPendingCommands() const;
    void OnCommand();
    void AddTimer(uint64_t timerId, const std::weak_ptr<MediaTimerCallback>& callbackRef);
    void RemoveTimer(uint64_t timerId);
    void StartTimer(uint64_t timerId, bool singleshot);
    void StopTimer(uint64_t timerId);
    void SetTimeout(uint64_t timerId, uint32_t timeoutMs);
private:
    const UVLoop _loop;
    const UVAsyncHandle _commandEvent;
    const UVAsyncHandle _stopEvent;
    ProtectedObj<absl::flat_hash_map<uint64_t, std::unique_ptr<TimerWrapper>>> _timers;
    ProtectedObj<CommandsQueue> _commands;
};

MediaTimerHandleUV::MediaTimerHandleUV(const std::shared_ptr<MediaTimerCallback>& callback,
                                       const std::shared_ptr<TimerCommandManager>& manager)
    : MediaTimerHandle(callback)
    , _managerRef(manager)
{
    manager->Add(TimerCommand(GetTimerId(), callback));
    SetTimeout(GetTimeout());
}

MediaTimerHandleUV::~MediaTimerHandleUV()
{
    MediaTimerHandleUV::Stop();
    if (const auto manager = _managerRef.lock()) {
        manager->Add(TimerCommand(GetTimerId()));
    }
}

void MediaTimerHandleUV::Start(bool singleshot)
{
    if (const auto manager = _managerRef.lock()) {
        manager->Add(TimerCommand(GetTimerId(), true, singleshot));
    }
}

void MediaTimerHandleUV::Stop()
{
    if (const auto manager = _managerRef.lock()) {
        manager->Add(TimerCommand(GetTimerId(), false));
    }
}

bool MediaTimerHandleUV::IsStarted() const
{
    if (const auto manager = _managerRef.lock()) {
        return manager->IsTimerStarted(GetTimerId());
    }
    return false;
}

void MediaTimerHandleUV::OnTimeoutChanged(uint32_t timeoutMs)
{
    MediaTimerHandle::OnTimeoutChanged(timeoutMs);
    SetTimeout(timeoutMs);
}

void MediaTimerHandleUV::SetTimeout(uint32_t timeoutMs)
{
    if (const auto manager = _managerRef.lock()) {
        manager->Add(TimerCommand(GetTimerId(), timeoutMs));
    }
}

MediaTimerHandleFactoryUV::MediaTimerHandleFactoryUV(const std::string& timerName,
                                                     std::shared_ptr<Impl> impl)
    : _timerName(timerName)
    , _impl(std::move(impl))
    , _thread(std::bind(&MediaTimerHandleFactoryUV::Run, this))
{
}

MediaTimerHandleFactoryUV::~MediaTimerHandleFactoryUV()
{
    if (!_cancelled.exchange(true)) {
        _impl->StopLoop();
    }
    if (_thread.joinable()) {
        _thread.join();
    }
}

std::unique_ptr<MediaTimerHandleFactory> MediaTimerHandleFactoryUV::
    Create(const std::string& timerName)
{
    std::unique_ptr<MediaTimerHandleFactory> factory;
    auto loop = UVLoop::CreateInitialized();
    if (loop.IsValid()) {
        auto impl = std::make_shared<Impl>(std::move(loop));
        factory.reset(new MediaTimerHandleFactoryUV(timerName, std::move(impl)));
    }
    return factory;
}

std::unique_ptr<MediaTimerHandle> MediaTimerHandleFactoryUV::
    CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback)
{
    if (!IsCancelled()) {
        return std::make_unique<MediaTimerHandleUV>(callback, _impl);
    }
    return nullptr;
}

void MediaTimerHandleFactoryUV::Run()
{
    if (!IsCancelled()) {
        if (!SetCurrentThreadPriority(ThreadPriority::High)) {
            MS_WARN_DEV_STD("failed to set high prioriry for '%s' timer", _timerName.c_str());
        }
        SetCurrentThreadName(_timerName);
        _impl->RunLoop();
    }
}

MediaTimerHandleFactoryUV::Impl::Impl(UVLoop loop)
    : _loop(std::move(loop))
    , _commandEvent(_loop.GetHandle(), OnCommand, this)
    , _stopEvent(_loop.GetHandle(), OnStop, this)
{
    MS_ASSERT(_loop, "wrong events loop");
}

MediaTimerHandleFactoryUV::Impl::~Impl()
{
    StopLoop();
    LOCK_WRITE_PROTECTED_OBJ(_commands);
    while (!_commands->empty()) {
        _commands->pop();
    }
}

int MediaTimerHandleFactoryUV::Impl::RunLoop()
{
    if (HasPendingCommands()) {
        OnCommand();
    }
    const auto result = uv_run(_loop.GetHandle(), UV_RUN_DEFAULT);
    LOCK_WRITE_PROTECTED_OBJ(_timers);
    _timers->clear();
    return result;
}

void MediaTimerHandleFactoryUV::Impl::StopLoop()
{
    _stopEvent.Invoke();
}

void MediaTimerHandleFactoryUV::Impl::Add(TimerCommand command)
{
    MS_ASSERT(command.GetTimerId(), "invalid timer ID");
    {
        LOCK_WRITE_PROTECTED_OBJ(_commands);
        _commands->push(std::move(command));
    }
    _commandEvent.Invoke();
}

bool MediaTimerHandleFactoryUV::Impl::IsTimerStarted(uint64_t timerId) const
{
    LOCK_READ_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        return it->second->IsActive();
    }
    return false;
}

bool MediaTimerHandleFactoryUV::Impl::HasPendingCommands() const
{
    LOCK_READ_PROTECTED_OBJ(_commands);
    return !_commands->empty();
}

void MediaTimerHandleFactoryUV::Impl::OnCommand(uv_async_t* handle)
{
    if (handle && handle->data) {
        reinterpret_cast<Impl*>(handle->data)->OnCommand();
    }
}

void MediaTimerHandleFactoryUV::Impl::OnStop(uv_async_t* handle)
{
    if (handle && handle->data) {
        const auto self = reinterpret_cast<Impl*>(handle->data);
        uv_stop(self->_loop.GetHandle());
    }
}

void MediaTimerHandleFactoryUV::Impl::OnCommand()
{
    CommandsQueue commands;
    {
        LOCK_WRITE_PROTECTED_OBJ(_commands);
        commands = _commands.Take();
    }
    while (!commands.empty()) {
        const auto& command = commands.front();
        switch (command.GetType()) {
            case TimerCommandType::Add:
                AddTimer(command.GetTimerId(), command.GetCallback());
                break;
            case TimerCommandType::Remove:
                RemoveTimer(command.GetTimerId());
                break;
            case TimerCommandType::Start:
                StartTimer(command.GetTimerId(), command.IsSingleshot());
                break;
            case TimerCommandType::Stop:
                StopTimer(command.GetTimerId());
                break;
            case TimerCommandType::SetTimeout:
                SetTimeout(command.GetTimerId(), command.GetTimeout());
                break;
            default:
                // TODO: log unhandled/unknown command
                break;
        }
        commands.pop();
    }
}

void MediaTimerHandleFactoryUV::Impl::AddTimer(uint64_t timerId,
                                   const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    if (!callbackRef.expired()) {
        auto timer = UVTimer::CreateInitialized(_loop.GetHandle());
        if (timer.IsValid()) {
            auto wrapper = std::make_unique<TimerWrapper>(callbackRef, timerId, std::move(timer));
            LOCK_WRITE_PROTECTED_OBJ(_timers);
            _timers->insert(std::make_pair(timerId, std::move(wrapper)));
        }
        else {
            // TODO: log error
        }
    }
}

void MediaTimerHandleFactoryUV::Impl::RemoveTimer(uint64_t timerId)
{
    LOCK_WRITE_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        _timers->erase(it);
    }
}

void MediaTimerHandleFactoryUV::Impl::StartTimer(uint64_t timerId, bool singleshot)
{
    LOCK_READ_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        it->second->Start(singleshot);
    }
}

void MediaTimerHandleFactoryUV::Impl::StopTimer(uint64_t timerId)
{
    LOCK_READ_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        it->second->Stop();
    }
}

void MediaTimerHandleFactoryUV::Impl::SetTimeout(uint64_t timerId, uint32_t timeoutMs)
{
    LOCK_READ_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        it->second->SetTimeout(timeoutMs);
    }
}

} // namespace RTC

namespace {

TimerWrapper::TimerWrapper(const std::weak_ptr<MediaTimerCallback>& callbackRef,
                           uint64_t timerId, UVTimer timer)
    : _callbackRef(callbackRef)
    , _timerId(timerId)
    , _timer(std::move(timer))
{
    _timer->data = this;
}

TimerWrapper::~TimerWrapper()
{
    Stop();
    _timer->data = nullptr;
}

void TimerWrapper::SetTimeout(uint32_t timeoutMs)
{
    if (_timeoutMs != timeoutMs) {
        const auto wasActive = IsActive();
        if (wasActive) {
            Stop();
        }
        _timeoutMs = timeoutMs;
        if (wasActive) {
            Start(_singleshot);
        }
    }
}

void TimerWrapper::Start(bool singleshot)
{
    _singleshot = singleshot;
    uv_timer_start(_timer.GetHandle(), [](uv_timer_t *handle) {
        if (handle && handle->data) {
            const auto self = reinterpret_cast<TimerWrapper*>(handle->data);
            self->OnFired();
        }
    }, _timeoutMs, std::max<uint64_t>(1U, _timeoutMs));
}

void TimerWrapper::Stop()
{
    uv_timer_stop(_timer.GetHandle());
}

bool TimerWrapper::IsActive() const
{
    return 0 != uv_is_active(reinterpret_cast<uv_handle_t*>(_timer.GetHandle()));
}

void TimerWrapper::Invoke()
{
    if (const auto callback = _callbackRef.lock()) {
        callback->OnEvent(_timerId);
    }
}

void TimerWrapper::OnFired()
{
    if (_singleshot) {
        Stop();
    }
    Invoke();
}

TimerCommand::TimerCommand(uint64_t timerId, const std::weak_ptr<MediaTimerCallback>& callbackRef)
    : _type(TimerCommandType::Add)
    , _timerId(timerId)
    , _data(callbackRef)
{
}

TimerCommand::TimerCommand(uint64_t timerId, bool start, bool singleshot)
    : _type(start ? TimerCommandType::Start : TimerCommandType::Stop)
    , _timerId(timerId)
{
    if (start) {
        _data = singleshot;
    }
}

TimerCommand::TimerCommand(uint64_t timerId, uint32_t timeoutMs)
    : _type(TimerCommandType::SetTimeout)
    , _timerId(timerId)
    , _data(timeoutMs)
{
}

TimerCommand::TimerCommand(uint64_t timerId)
    : _type(TimerCommandType::Remove)
    , _timerId(timerId)
{
}

std::weak_ptr<MediaTimerCallback> TimerCommand::GetCallback() const
{
    return std::get<std::weak_ptr<MediaTimerCallback>>(_data);
}

uint32_t TimerCommand::GetTimeout() const
{
    return std::get<uint32_t>(_data);
}

bool TimerCommand::IsSingleshot() const
{
    return std::get<bool>(_data);
}

}
