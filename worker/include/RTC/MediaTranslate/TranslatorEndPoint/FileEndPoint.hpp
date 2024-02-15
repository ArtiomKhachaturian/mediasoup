#pragma once // 
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include <atomic>
#include <string>

namespace RTC
{

class MediaTimer;

class FileEndPoint : public TranslatorEndPoint
{
    class TimerCallback;
public:
    FileEndPoint(std::string fileName);
    ~FileEndPoint() final;
    const std::string& GetFileName() const { return _fileName; }
    uint32_t GetIntervalBetweenTranslationsMs() const;
    void SetIntervalBetweenTranslationsMs(uint32_t intervalMs);
    uint32_t GetConnectionDelay() const;
    void SetConnectionDelay(uint32_t delaylMs);
    bool IsValid() const { return 0UL != _timerId; }
protected:
    // impl. of TranslatorEndPoint
    bool IsConnected() const final;
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const MemoryBuffer& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    const bool _fileIsValid;
    const std::string _fileName;
    const std::shared_ptr<TimerCallback> _callback;
    const std::unique_ptr<MediaTimer> _timer;
    const uint64_t _timerId;
    std::atomic<uint32_t> _intervalBetweenTranslationsMs = 1000; // 1 sec
    std::atomic<uint32_t> _connectionDelaylMs = 500; // 0.5 sec
};

} // namespace RTC
