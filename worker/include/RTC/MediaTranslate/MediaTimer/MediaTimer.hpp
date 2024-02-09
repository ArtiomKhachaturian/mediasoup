#pragma once
#include "ProtectedObj.hpp"
#include "absl/container/flat_hash_map.h"
#include <string>
#include <memory>

namespace RTC
{

class MediaTimerCallback;
class MediaTimerHandle;
class MediaTimerHandleFactory;

class MediaTimer
{
public:
    MediaTimer(std::string timerName = std::string());
    ~MediaTimer();
    uint64_t RegisterTimer(const std::weak_ptr<MediaTimerCallback>& callbackRef);
    void UnregisterTimer(uint64_t timerId);
    // time-out in milliseconds, previous invokes will discarded
    void SetTimeout(uint64_t timerId, uint64_t timeoutMs);
    void Start(uint64_t timerId, bool singleshot);
    void Stop(uint64_t timerId);
private:
	const std::unique_ptr<MediaTimerHandleFactory> _factory;
    const std::string _timerName;
	// key is timer ID
	ProtectedObj<absl::flat_hash_map<uint64_t, std::unique_ptr<MediaTimerHandle>>> _handles;
};

} // namespace RTC
