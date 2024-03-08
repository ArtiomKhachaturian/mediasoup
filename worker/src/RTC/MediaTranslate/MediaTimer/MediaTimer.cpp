#define MS_CLASS "RTC::MediaTimer"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/FunctorCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactoryUV.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include <unordered_map>

namespace {

using namespace RTC;

class SingleshotOwner
{
public:
    virtual ~SingleshotOwner() = default;
    virtual void Unregister(uint64_t timerId) = 0;
};

class SingleShotCallback : public MediaTimerCallback
{
public:
    SingleShotCallback(const std::shared_ptr<MediaTimerCallback>& callback,
                       const std::weak_ptr<SingleshotOwner>& ownerRef);
    // impl. of MediaTimerCallback
    void OnCallbackRegistered(uint64_t timerId, bool registered) final;
    void OnEvent(uint64_t timerId) final;
private:
    const std::shared_ptr<MediaTimerCallback> _callback;
    const std::weak_ptr<SingleshotOwner> _ownerRef;
};

}

namespace RTC
{

class MediaTimer::Impl : public SingleshotOwner
{
    using HandlesMap = std::unordered_map<uint64_t, std::shared_ptr<MediaTimerHandle>>;
public:
    ~Impl();
    static std::shared_ptr<Impl> Create(std::string timerName);
    void SetTimeout(uint64_t timerId, uint32_t timeoutMs);
    void Start(uint64_t timerId, bool singleshot);
    bool IsStarted(uint64_t timerId) const;
    std::optional<uint32_t> GetTimeout(uint64_t timerId) const;
    void Stop(uint64_t timerId);
    uint64_t Register(const std::shared_ptr<MediaTimerCallback>& callback,
                      const std::optional<std::pair<uint32_t, bool>>& start = std::nullopt);
    void UnregisterAll();
    // impl. of SingleshotOwner
    void Unregister(uint64_t timerId) final;
private:
    Impl(std::unique_ptr<MediaTimerHandleFactory> factory, std::string timerName);
    std::shared_ptr<MediaTimerHandle> GetHandle(uint64_t timerId) const;
    static std::unique_ptr<MediaTimerHandleFactory> CreateFactory(const std::string& timerName);
private:
    const std::unique_ptr<MediaTimerHandleFactory> _factory;
    const std::string _timerName;
    // key is timer ID
    ProtectedObj<HandlesMap, std::mutex> _handles;
};

MediaTimer::MediaTimer(std::string timerName)
    : _impl(Impl::Create(std::move(timerName)))
{
}

MediaTimer::~MediaTimer()
{
    if (_impl) {
        _impl->UnregisterAll();
    }
}

uint64_t MediaTimer::Register(const std::shared_ptr<MediaTimerCallback>& callback)
{
    return _impl ? _impl->Register(callback) : 0ULL;
}

uint64_t MediaTimer::Register(std::function<void(uint64_t)> onEvent)
{
    return _impl && onEvent ? Register(CreateCallback(std::move(onEvent))) : 0ULL;
}

uint64_t MediaTimer::RegisterAndStart(const std::shared_ptr<MediaTimerCallback>& callback,
                                      uint32_t timeoutMs, bool singleshot)
{
    return _impl ? _impl->Register(callback, std::make_pair(timeoutMs, singleshot)) : 0ULL;
}

uint64_t MediaTimer::RegisterAndStart(std::function<void(uint64_t)> onEvent,
                                      uint32_t timeoutMs, bool singleshot)
{
    return _impl ? _impl->Register(CreateCallback(std::move(onEvent)),
                                   std::make_pair(timeoutMs, singleshot)) : 0ULL;
}

void MediaTimer::Unregister(uint64_t timerId)
{
    if (_impl) {
        _impl->Unregister(timerId);
    }
}

void MediaTimer::SetTimeout(uint64_t timerId, uint32_t timeoutMs)
{
    if (_impl) {
        _impl->SetTimeout(timerId, timeoutMs);
    }
}

void MediaTimer::Start(uint64_t timerId, bool singleshot)
{
    if (_impl) {
        _impl->Start(timerId, singleshot);
    }
}

void MediaTimer::Stop(uint64_t timerId)
{
    if (_impl) {
        _impl->Stop(timerId);
    }
}

bool MediaTimer::IsStarted(uint64_t timerId) const
{
    return _impl && _impl->IsStarted(timerId);
}

std::optional<uint32_t> MediaTimer::GetTimeout(uint64_t timerId) const
{
    return _impl ? _impl->GetTimeout(timerId) : std::nullopt;
}

uint64_t MediaTimer::Singleshot(uint32_t afterMs, const std::shared_ptr<MediaTimerCallback>& callback)
{
    if (_impl && callback) {
        const auto singleshotCallback = std::make_shared<SingleShotCallback>(callback, _impl);
        if (const auto timerId = _impl->Register(singleshotCallback)) {
            _impl->SetTimeout(timerId, afterMs);
            _impl->Start(timerId, true);
            return timerId;
        }
    }
    return 0ULL;
}

uint64_t MediaTimer::Singleshot(uint32_t afterMs, std::function<void(void)> onEvent)
{
    if (_impl && onEvent) {
        return Singleshot(afterMs, CreateCallback([onEvent = std::move(onEvent)](uint64_t) {
            onEvent();
        }));
    }
    return 0ULL;
}

std::shared_ptr<MediaTimerCallback> MediaTimer::CreateCallback(std::function<void(uint64_t)> onEvent)
{
    if (onEvent) {
        return std::make_shared<FunctorCallback>(std::move(onEvent));
    }
    return nullptr;
}

MediaTimer::Impl::Impl(std::unique_ptr<MediaTimerHandleFactory> factory, std::string timerName)
    : _factory(std::move(factory))
    , _timerName(std::move(timerName))
{
}

MediaTimer::Impl::~Impl()
{
    UnregisterAll();
}

std::shared_ptr<MediaTimer::Impl> MediaTimer::Impl::Create(std::string timerName)
{
    std::shared_ptr<Impl> impl;
    auto factory = CreateFactory(timerName);
    if (factory) {
        impl.reset(new Impl(std::move(factory), std::move(timerName)));
    }
    else {
        MS_ERROR_STD("failed to create handle factory for media timer %s", timerName.c_str());
    }
    return impl;
}

void MediaTimer::Impl::SetTimeout(uint64_t timerId, uint32_t timeoutMs)
{
    if (const auto handle = GetHandle(timerId)) {
        handle->SetTimeout(timeoutMs);
    }
}

void MediaTimer::Impl::Start(uint64_t timerId, bool singleshot)
{
    if (const auto handle = GetHandle(timerId)) {
        handle->Start(singleshot);
    }
}

bool MediaTimer::Impl::IsStarted(uint64_t timerId) const
{
    const auto handle = GetHandle(timerId);
    return handle && handle->IsStarted();
}

std::optional<uint32_t> MediaTimer::Impl::GetTimeout(uint64_t timerId) const
{
    if (const auto handle = GetHandle(timerId)) {
        return handle->GetTimeout();
    }
    return std::nullopt;
}

void MediaTimer::Impl::Stop(uint64_t timerId)
{
    if (const auto handle = GetHandle(timerId)) {
        handle->Stop();
    }
}

uint64_t MediaTimer::Impl::Register(const std::shared_ptr<MediaTimerCallback>& callback,
                                    const std::optional<std::pair<uint32_t, bool>>& start)
{
    uint64_t timerId = 0ULL;
    if (callback) {
        if (auto handle = _factory->CreateHandle(callback)) {
            timerId = handle->GetId();
            {
                LOCK_WRITE_PROTECTED_OBJ(_handles);
                _handles->insert(std::make_pair(timerId, handle));
            }
            if (start) {
                handle->SetTimeout(start->first);
            }
            callback->OnCallbackRegistered(timerId, true);
            if (start) {
                handle->Start(start->second);
            }
        }
        else {
            MS_ERROR_STD("failed to create handle for media timer %s", _timerName.c_str());
        }
    }
    return timerId;
}

void MediaTimer::Impl::UnregisterAll()
{
    HandlesMap handles;
    {
        LOCK_WRITE_PROTECTED_OBJ(_handles);
        handles = _handles.Take();
    }
    for (auto it = handles.begin(); it != handles.end(); ++it) {
        _factory->DestroyHandle(std::move(it->second));
    }
}

void MediaTimer::Impl::Unregister(uint64_t timerId)
{
    if (timerId) {
        std::shared_ptr<MediaTimerHandle> handle;
        {
            LOCK_WRITE_PROTECTED_OBJ(_handles);
            const auto it = _handles->find(timerId);
            if (it != _handles->end()) {
                handle = std::move(it->second);
                _handles->erase(it);
            }
        }
        _factory->DestroyHandle(std::move(handle));
    }
}

std::unique_ptr<MediaTimerHandleFactory> MediaTimer::Impl::
    CreateFactory(const std::string& timerName)
{
    return MediaTimerHandleFactoryUV::Create(timerName);
}

std::shared_ptr<MediaTimerHandle> MediaTimer::Impl::GetHandle(uint64_t timerId) const
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            return it->second;
        }
    }
    return nullptr;
}

void MediaTimerHandleFactory::DestroyHandle(std::shared_ptr<MediaTimerHandle> handle)
{
    if (handle) {
        handle->Stop();
        handle.reset();
    }
}

MediaTimerHandle::MediaTimerHandle(const std::shared_ptr<MediaTimerCallback>& callback)
    : _callback(callback)
{
}

void MediaTimerHandle::SetTimeout(uint32_t timeoutMs)
{
    if (timeoutMs != _timeoutMs.exchange(timeoutMs)) {
        OnTimeoutChanged(timeoutMs);
    }
}

MediaTimerHandle::~MediaTimerHandle()
{
    _callback->OnCallbackRegistered(GetId(), false);
}

} // namespace RTC

namespace {

SingleShotCallback::SingleShotCallback(const std::shared_ptr<MediaTimerCallback>& callback,
                                       const std::weak_ptr<SingleshotOwner>& ownerRef)
    : _callback(callback)
    , _ownerRef(ownerRef)
{
}

void SingleShotCallback::OnCallbackRegistered(uint64_t timerId, bool registered)
{
    MediaTimerCallback::OnCallbackRegistered(timerId, registered);
    _callback->OnCallbackRegistered(timerId, registered);
}

void SingleShotCallback::OnEvent(uint64_t timerId)
{
    _callback->OnEvent(timerId);
    if (const auto owner = _ownerRef.lock()) {
        owner->Unregister(timerId);
    }
}

}
