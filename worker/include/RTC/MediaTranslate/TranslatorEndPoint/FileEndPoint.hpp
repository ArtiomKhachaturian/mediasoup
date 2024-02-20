#pragma once // 
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include <atomic>
#include <optional>
#include <string>

namespace RTC
{

class MediaTimer;

class FileEndPoint : public TranslatorEndPoint
{
    class TimerCallback;
public:
    FileEndPoint(std::string fileName,
                 std::string ownerId = std::string(),
                 uint32_t intervalBetweenTranslationsMs = 1000, // 1sec
                 uint32_t connectionDelaylMs = 500U, // 0.5 sec
                 const std::optional<uint32_t>& disconnectAfterMs = std::nullopt,
                 const std::shared_ptr<MediaTimer>& timer = nullptr);
    ~FileEndPoint() final;
    static uint64_t GetInstancesCount() { return _instances.load(); }
    bool IsValid() const { return 0UL != _timerId; }
    // impl. of TranslatorEndPoint
    bool IsConnected() const final;
protected:
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const std::shared_ptr<MemoryBuffer>& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    static inline std::atomic<uint64_t> _instances = 0ULL;
    const bool _fileIsValid;
    const std::shared_ptr<TimerCallback> _callback;
    const std::shared_ptr<MediaTimer> _timer;
    const uint64_t _timerId;
    const uint32_t _intervalBetweenTranslationsMs;
    const uint32_t _connectionDelaylMs;
    uint64_t _disconnectedTimerId = 0U;
};

} // namespace RTC
