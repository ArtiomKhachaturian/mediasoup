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
    class SingleShotCallback;
public:
    MediaTimer(std::string timerName = std::string());
    ~MediaTimer();
    uint64_t RegisterTimer(const std::shared_ptr<MediaTimerCallback>& callback);
    void UnregisterTimer(uint64_t timerId);
    // time-out in milliseconds, previous invokes will discarded
    void SetTimeout(uint64_t timerId, uint32_t timeoutMs);
    void Start(uint64_t timerId, bool singleshot);
    void Stop(uint64_t timerId);
    bool IsStarted(uint64_t timerId) const;
    bool Singleshot(uint32_t afterMs, const std::shared_ptr<MediaTimerCallback>& callback);
private:
	const std::shared_ptr<MediaTimerHandleFactory> _factory;
    const std::string _timerName;
	// key is timer ID
	ProtectedObj<absl::flat_hash_map<uint64_t, std::unique_ptr<MediaTimerHandle>>> _handles;
};

} // namespace RTC
