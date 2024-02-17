#define MS_CLASS "RTC::MediaTimerHandleFactoryUnix"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "UVAsyncHandle.hpp"
#include "Logger.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>
#include <variant>
#include <queue>
#include <thread>
#ifdef _WIN32
#include <Windows.h>
#endif

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
    TimerWrapper(const std::weak_ptr<MediaTimerCallback>& callbackRef, UVTimer timer);
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
    const UVTimer _timer;
    bool _singleshot;
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
    using CommandsQueue = std::queue<TimerCommand>;
public:
    TimerCommandManager(UVLoop loop);
    ~TimerCommandManager();
    int RunLoop();
    void StopLoop();
    void Add(TimerCommand command);
    bool IsTimerStarted(uint64_t timerId) const;
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

class MediaTimerHandleFactoryUV : public MediaTimerHandleFactory
{
public:
    MediaTimerHandleFactoryUV(const std::string& timerName, UVLoop loop);
    ~MediaTimerHandleFactoryUV() final;
    // impl. of MediaTimerHandleFactory
    std::unique_ptr<MediaTimerHandle> CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback) final;
private:
    void Run();
    bool IsCancelled() const { return _cancelled.load(); }
    static void SetThreadName(const std::string& timerName);
private:
    const std::string _timerName;
    const std::shared_ptr<TimerCommandManager> _commandsManager;
    std::thread _thread;
    std::atomic_bool _cancelled = false;
};

std::unique_ptr<MediaTimerHandleFactory> MediaTimerHandleFactory::Create(const std::string& timerName)
{
    auto loop = UVLoop::CreateInitialized();
    if (loop.IsValid()) {
        return std::make_unique<MediaTimerHandleFactoryUV>(timerName, std::move(loop));
    }
    return nullptr;
}

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

MediaTimerHandleFactoryUV::MediaTimerHandleFactoryUV(const std::string& timerName, UVLoop loop)
    : _timerName(timerName)
    , _commandsManager(std::make_shared<TimerCommandManager>(std::move(loop)))
    , _thread(std::bind(&MediaTimerHandleFactoryUV::Run, this))
{
}

MediaTimerHandleFactoryUV::~MediaTimerHandleFactoryUV()
{
    if (!_cancelled.exchange(true)) {
        _commandsManager->StopLoop();
    }
    if (_thread.joinable()) {
        _thread.join();
    }
}

std::unique_ptr<MediaTimerHandle> MediaTimerHandleFactoryUV::
    CreateHandle(const std::shared_ptr<MediaTimerCallback>& callback)
{
    if (!IsCancelled()) {
        return std::make_unique<MediaTimerHandleUV>(callback, _commandsManager);
    }
    return nullptr;
}

void MediaTimerHandleFactoryUV::Run()
{
    if (!IsCancelled()) {
        SetThreadName(_timerName);
        _commandsManager->RunLoop();
    }
}

void MediaTimerHandleFactoryUV::SetThreadName(const std::string& timerName)
{
    if (!timerName.empty()) {
#ifdef WIN32
        struct
        {
            DWORD dwType;
            LPCSTR szName;
            DWORD dwThreadID;
            DWORD dwFlags;
        } threadname_info = {0x1000, timerName.c_str(), static_cast<DWORD>(-1), 0};

        __try {
            ::RaiseException(0x406D1388, 0, sizeof(threadname_info) / sizeof(DWORD), reinterpret_cast<ULONG_PTR*>(&threadname_info));
        } __except (EXCEPTION_EXECUTE_HANDLER) { /* NOLINT */
        }
#elif defined(__APPLE__)
        if (0 != pthread_setname_np(timerName.c_str())) {
            MS_WARN_DEV_STD("failed to set name %s for timer's thread", timerName.c_str());
        }
#elif defined(__unix__) || defined(__linux__) || defined(_POSIX_VERSION)
        prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(timerName.c_str())); // NOLINT
#endif
    }
}

} // namespace RTC

namespace {

TimerWrapper::TimerWrapper(const std::weak_ptr<MediaTimerCallback>& callbackRef, UVTimer timer)
    : _callbackRef(callbackRef)
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
    uv_timer_set_repeat(_timer.GetHandle(), timeoutMs);
}

void TimerWrapper::Start(bool singleshot)
{
    const auto interval = uv_timer_get_repeat(_timer.GetHandle());
    _singleshot = singleshot;
    uv_timer_start(_timer.GetHandle(), [](uv_timer_t *handle) {
        if (handle && handle->data) {
            const auto self = reinterpret_cast<TimerWrapper*>(handle->data);
            self->OnFired();
        }
    }, interval, std::max(1ULL, interval));
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
        callback->OnEvent();
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

TimerCommandManager::TimerCommandManager(UVLoop loop)
    : _loop(std::move(loop))
    , _commandEvent(_loop.GetHandle(), OnCommand, this)
    , _stopEvent(_loop.GetHandle(), OnStop, this)
{
    MS_ASSERT(_loop, "wrong events loop");
}

TimerCommandManager::~TimerCommandManager()
{
    StopLoop();
    LOCK_WRITE_PROTECTED_OBJ(_commands);
    while (!_commands->empty()) {
        _commands->pop();
    }
}

int TimerCommandManager::RunLoop()
{
    if (HasPendingCommands()) {
        OnCommand();
    }
    const auto result = uv_run(_loop.GetHandle(), UV_RUN_DEFAULT);
    LOCK_WRITE_PROTECTED_OBJ(_timers);
    _timers->clear();
    return result;
}

void TimerCommandManager::StopLoop()
{
    _stopEvent.Invoke();
}

void TimerCommandManager::Add(TimerCommand command)
{
    MS_ASSERT(command.GetTimerId(), "invalid timer ID");
    {
        LOCK_WRITE_PROTECTED_OBJ(_commands);
        _commands->push(std::move(command));
    }
    _commandEvent.Invoke();
}

bool TimerCommandManager::IsTimerStarted(uint64_t timerId) const
{
    LOCK_READ_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        return it->second->IsActive();
    }
    return false;
}

bool TimerCommandManager::HasPendingCommands() const
{
    LOCK_READ_PROTECTED_OBJ(_commands);
    return !_commands->empty();
}

void TimerCommandManager::OnCommand(uv_async_t* handle)
{
    if (handle && handle->data) {
        reinterpret_cast<TimerCommandManager*>(handle->data)->OnCommand();
    }
}

void TimerCommandManager::OnStop(uv_async_t* handle)
{
    if (handle && handle->data) {
        const auto self = reinterpret_cast<TimerCommandManager*>(handle->data);
        uv_stop(self->_loop.GetHandle());
    }
}

void TimerCommandManager::OnCommand()
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

void TimerCommandManager::AddTimer(uint64_t timerId,
                                   const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    if (!callbackRef.expired()) {
        auto timer = UVTimer::CreateInitialized(_loop.GetHandle());
        if (timer.IsValid()) {
            auto wrapper = std::make_unique<TimerWrapper>(callbackRef, std::move(timer));
            LOCK_WRITE_PROTECTED_OBJ(_timers);
            _timers->insert(std::make_pair(timerId, std::move(wrapper)));
        }
        else {
            // TODO: log error
        }
    }
}

void TimerCommandManager::RemoveTimer(uint64_t timerId)
{
    LOCK_WRITE_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        _timers->erase(it);
    }
}

void TimerCommandManager::StartTimer(uint64_t timerId, bool singleshot)
{
    LOCK_READ_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        it->second->Start(singleshot);
    }
}

void TimerCommandManager::StopTimer(uint64_t timerId)
{
    LOCK_READ_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        it->second->Stop();
    }
}

void TimerCommandManager::SetTimeout(uint64_t timerId, uint32_t timeoutMs)
{
    LOCK_READ_PROTECTED_OBJ(_timers);
    const auto it = _timers->find(timerId);
    if (it != _timers->end()) {
        it->second->SetTimeout(timeoutMs);
    }
}

}
