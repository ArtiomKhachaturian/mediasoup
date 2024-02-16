#define MS_CLASS "RTC::MediaTimer"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandle.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerHandleFactory.hpp"
#include "Logger.hpp"

namespace RTC
{

class MediaTimer::SingleShotCallback : public MediaTimerCallback
{
public:
    SingleShotCallback(const std::shared_ptr<MediaTimerCallback>& callback);
    void SetStopParameters(uint64_t timerId, MediaTimer* timer);
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    const std::shared_ptr<MediaTimerCallback> _callback;
    uint64_t _timerId = 0ULL;
    MediaTimer* _timer = nullptr;
};

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

uint64_t MediaTimer::RegisterTimer(const std::shared_ptr<MediaTimerCallback>& callback)
{
    uint64_t timerId = 0ULL;
    if (_factory && callback) {
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

void MediaTimer::SetTimeout(uint64_t timerId, uint32_t timeoutMs)
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
            return it->second->IsStarted();
        }
    }
    return false;
}

bool MediaTimer::Singleshot(uint32_t afterMs, const std::shared_ptr<MediaTimerCallback>& callback)
{
    if (_factory && callback) {
        const auto singleshotCallback = std::make_shared<SingleShotCallback>(callback);
        if (const auto timerId = RegisterTimer(singleshotCallback)) {
            singleshotCallback->SetStopParameters(timerId, this);
            SetTimeout(timerId, afterMs);
            Start(timerId, true);
            return true;
        }
    }
    return false;
}


MediaTimer::SingleShotCallback::SingleShotCallback(const std::shared_ptr<MediaTimerCallback>& callback)
    : _callback(callback)
{
}

void MediaTimer::SingleShotCallback::OnEvent()
{
    _callback->OnEvent();
    if (_timerId && _timer) {
        _timer->Stop(_timerId);
    }
}

void MediaTimer::SingleShotCallback::SetStopParameters(uint64_t timerId, MediaTimer* timer)
{
    _timerId = timerId;
    _timer = timer;
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
