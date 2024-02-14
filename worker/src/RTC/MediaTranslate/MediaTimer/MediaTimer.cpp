#define MS_CLASS "RTC::MediaTimer"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaTimer::MediaTimer(std::string timerName)
    : _factory(MediaTimerHandleFactory::Create(timerName))
    , _timerName(std::move(timerName))
{
    if (!_factory) {
        MS_ERROR_STD("failed to create handle factory for media timer %s", _timerName.c_str());
    }
}

MediaTimer::~MediaTimer()
{
    if (_factory) {
        LOCK_WRITE_PROTECTED_OBJ(_handles);
        for (auto it = _handles->begin(); it != _handles->end(); ++it) {
            _factory->DestroyHandle(std::move(it->second));
        }
        _handles->clear();
    }
}

uint64_t MediaTimer::RegisterTimer(const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    uint64_t timerId = 0ULL;
    if (_factory && !callbackRef.expired()) {
        if (auto handle = _factory->CreateHandle(callbackRef)) {
            timerId = reinterpret_cast<uint64_t>(handle.get());
            LOCK_WRITE_PROTECTED_OBJ(_handles);
            _handles.Ref()[timerId] = std::move(handle);
        }
        else {
            MS_ERROR_STD("failed to create handle for media timer %s", _timerName.c_str());
        }
    }
    return timerId;
}

void MediaTimer::UnregisterTimer(uint64_t timerId)
{
    if (_factory && timerId) {
        LOCK_WRITE_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            _factory->DestroyHandle(std::move(it->second));
            _handles->erase(it);
        }
    }
}

void MediaTimer::SetTimeout(uint64_t timerId, uint64_t timeoutMs)
{
    if (_factory && timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->SetTimeout(timeoutMs);
        }
    }
}

void MediaTimer::Start(uint64_t timerId, bool singleshot)
{
    if (_factory && timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Start(singleshot);
        }
    }
}

void MediaTimer::Stop(uint64_t timerId)
{
    if (_factory && timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Stop();
        }
    }
}

bool MediaTimer::IsStarted(uint64_t timerId) const
{
    if (_factory && timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->IsStarted();
        }
    }
    return false;
}

void MediaTimerHandleFactory::DestroyHandle(std::unique_ptr<MediaTimerHandle> handle)
{
    if (handle) {
        handle->Stop();
        handle.reset();
    }
}

MediaTimerHandle::MediaTimerHandle(const std::weak_ptr<MediaTimerCallback>& callbackRef)
    : _callbackRef(callbackRef)
{
}

void MediaTimerHandle::SetTimeout(uint64_t timeoutMs)
{
    if (timeoutMs != _timeoutMs.exchange(timeoutMs)) {
        OnTimeoutChanged(timeoutMs);
    }
}

std::shared_ptr<MediaTimerCallback> MediaTimerHandle::GetCallback() const
{
    return GetCallbackRef().lock();
}

const std::weak_ptr<MediaTimerCallback>& MediaTimerHandle::GetCallbackRef() const
{
    return _callbackRef;
}

bool MediaTimerHandle::IsCallbackValid() const
{
    return !GetCallbackRef().expired();
}

} // namespace RTC
