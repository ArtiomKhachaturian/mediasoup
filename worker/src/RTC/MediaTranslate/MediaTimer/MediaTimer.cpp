#define MS_CLASS "RTC::MediaTimer"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/FunctorCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactoryUV.hpp"
#ifdef __APPLE__
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactoryApple.hpp"
#endif
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include "absl/container/flat_hash_map.h"

namespace {

using namespace RTC;

class SingleshotOwner
{
public:
    virtual ~SingleshotOwner() = default;
    virtual void UnregisterTimer(uint64_t timerId) = 0;
};

class SingleShotCallback : public MediaTimerCallback
{
public:
    SingleShotCallback(const std::shared_ptr<MediaTimerCallback>& callback,
                       const std::weak_ptr<SingleshotOwner>& ownerRef);
    void SetTimerId(uint64_t timerId);
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    const std::shared_ptr<MediaTimerCallback> _callback;
    const std::weak_ptr<SingleshotOwner> _ownerRef;
    uint64_t _timerId = 0ULL;
};

}

namespace RTC
{

class MediaTimer::Impl : public SingleshotOwner
{
public:
    ~Impl() { UnregisterAllTimers(); }
    static std::shared_ptr<Impl> Create(std::string timerName);
    void SetTimeout(uint64_t timerId, uint32_t timeoutMs);
    void Start(uint64_t timerId, bool singleshot);
    bool IsStarted(uint64_t timerId) const;
    void Stop(uint64_t timerId);
    uint64_t RegisterTimer(const std::shared_ptr<MediaTimerCallback>& callback);
    uint64_t RegisterTimer(std::function<void(void)> onEvent);
    void UnregisterAllTimers();
    // impl. of SingleshotOwner
    void UnregisterTimer(uint64_t timerId) final;
private:
    Impl(std::unique_ptr<MediaTimerHandleFactory> factory, std::string timerName);
    static std::unique_ptr<MediaTimerHandleFactory> CreateFactory(const std::string& timerName);
private:
    const std::unique_ptr<MediaTimerHandleFactory> _factory;
    const std::string _timerName;
    // key is timer ID
    ProtectedObj<absl::flat_hash_map<uint64_t, std::unique_ptr<MediaTimerHandle>>> _handles;
};

MediaTimer::MediaTimer(std::string timerName)
    : _impl(Impl::Create(std::move(timerName)))
{
}

MediaTimer::~MediaTimer()
{
    if (_impl) {
        _impl->UnregisterAllTimers();
    }
}

uint64_t MediaTimer::RegisterTimer(const std::shared_ptr<MediaTimerCallback>& callback)
{
    return _impl ? _impl->RegisterTimer(callback) : 0ULL;
}

uint64_t MediaTimer::RegisterTimer(std::function<void(void)> onEvent)
{
    return _impl && onEvent ? RegisterTimer(CreateCallback(std::move(onEvent))) : 0ULL;
}

void MediaTimer::UnregisterTimer(uint64_t timerId)
{
    if (_impl) {
        _impl->UnregisterTimer(timerId);
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

uint64_t MediaTimer::Singleshot(uint32_t afterMs, const std::shared_ptr<MediaTimerCallback>& callback)
{
    if (_impl && callback) {
        const auto singleshotCallback = std::make_shared<SingleShotCallback>(callback, _impl);
        if (const auto timerId = _impl->RegisterTimer(singleshotCallback)) {
            singleshotCallback->SetTimerId(timerId);
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
        return Singleshot(afterMs, CreateCallback(std::move(onEvent)));
    }
    return 0ULL;
}

std::shared_ptr<MediaTimerCallback> MediaTimer::CreateCallback(std::function<void(void)> onEvent)
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

void MediaTimer::Impl::Start(uint64_t timerId, bool singleshot)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Start(singleshot);
        }
    }
}

bool MediaTimer::Impl::IsStarted(uint64_t timerId) const
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            return it->second->IsStarted();
        }
    }
    return false;
}

void MediaTimer::Impl::Stop(uint64_t timerId)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Stop();
        }
    }
}

uint64_t MediaTimer::Impl::RegisterTimer(const std::shared_ptr<MediaTimerCallback>& callback)
{
    uint64_t timerId = 0ULL;
    if (callback) {
        if (auto handle = _factory->CreateHandle(callback)) {
            timerId = reinterpret_cast<uint64_t>(handle.get());
            LOCK_WRITE_PROTECTED_OBJ(_handles);
            _handles->insert(std::make_pair(timerId, std::move(handle)));
        }
        else {
            MS_ERROR_STD("failed to create handle for media timer %s", _timerName.c_str());
        }
    }
    return timerId;
}

void MediaTimer::Impl::UnregisterAllTimers()
{
    LOCK_WRITE_PROTECTED_OBJ(_handles);
    for (auto it = _handles->begin(); it != _handles->end(); ++it) {
        _factory->DestroyHandle(std::move(it->second));
    }
    _handles->clear();
}

void MediaTimer::Impl::UnregisterTimer(uint64_t timerId)
{
    if (timerId) {
        LOCK_WRITE_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            _factory->DestroyHandle(std::move(it->second));
            _handles->erase(it);
        }
    }
}

std::unique_ptr<MediaTimerHandleFactory> MediaTimer::Impl::
    CreateFactory(const std::string& timerName)
{
    std::unique_ptr<MediaTimerHandleFactory> factory;
#ifdef __APPLE__
    factory = MediaTimerHandleFactoryApple::Create(timerName);
#endif
    if (!factory) {
        factory = MediaTimerHandleFactoryUV::Create(timerName);
    }
    return factory;
}

void MediaTimer::Impl::SetTimeout(uint64_t timerId, uint32_t timeoutMs)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->SetTimeout(timeoutMs);
        }
    }
}

void MediaTimerHandleFactory::DestroyHandle(std::unique_ptr<MediaTimerHandle> handle)
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

} // namespace RTC

namespace {

SingleShotCallback::SingleShotCallback(const std::shared_ptr<MediaTimerCallback>& callback,
                                       const std::weak_ptr<SingleshotOwner>& ownerRef)
    : _callback(callback)
    , _ownerRef(ownerRef)
{
}

void SingleShotCallback::SetTimerId(uint64_t timerId)
{
    _timerId = timerId;
}

void SingleShotCallback::OnEvent()
{
    _callback->OnEvent();
    if (_timerId) {
        if (const auto owner = _ownerRef.lock()) {
            owner->UnregisterTimer(_timerId);
        }
    }
}

}
