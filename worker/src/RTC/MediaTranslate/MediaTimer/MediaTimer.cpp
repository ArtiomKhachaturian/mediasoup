#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"

namespace RTC
{

MediaTimer::MediaTimer()
    : _factory(MediaTimerHandleFactory::Create())
{
}

MediaTimer::~MediaTimer()
{
    LOCK_WRITE_PROTECTED_OBJ(_handles);
    for (auto it = _handles->begin(); it != _handles->end(); ++it) {
        it->second->Stop();
    }
    _handles->clear();
}

uint64_t MediaTimer::RegisterTimer(const std::weak_ptr<MediaTimerCallback>& callbackRef)
{
    if (!callbackRef.expired() && _factory) {
        if (auto handle = _factory->CreateHandle(callbackRef)) {
            const auto timerId = reinterpret_cast<uint64_t>(handle.get());
            {
                LOCK_WRITE_PROTECTED_OBJ(_handles);
                _handles.Ref()[timerId] = std::move(handle);
            }
            return timerId;
        }
    }
    return 0UL;
}

void MediaTimer::UnregisterTimer(uint64_t timerId)
{
    if (timerId) {
        LOCK_WRITE_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Stop();
            _handles->erase(it);
        }
    }
}

void MediaTimer::SetTimeout(uint64_t timerId, uint64_t timeoutMs)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->SetTimeout(timeoutMs);
        }
    }
}

void MediaTimer::Start(uint64_t timerId, bool singleshot)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Start(singleshot);
        }
    }
}

void MediaTimer::Stop(uint64_t timerId)
{
    if (timerId) {
        LOCK_READ_PROTECTED_OBJ(_handles);
        const auto it = _handles->find(timerId);
        if (it != _handles->end()) {
            it->second->Stop();
        }
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

} // namespace RTC
