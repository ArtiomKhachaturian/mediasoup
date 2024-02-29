#pragma once // 
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include <atomic>
#include <optional>
#include <string>

namespace RTC
{

class MediaTimer;
class BufferAllocator;

class FileEndPoint : public TranslatorEndPoint
{
    class TimerCallback;
public:
    FileEndPoint(std::string ownerId,
                 const std::shared_ptr<BufferAllocator>& allocator = nullptr,
                 const std::shared_ptr<MediaTimer>& timer = nullptr);
    ~FileEndPoint() final;
    static uint64_t GetInstancesCount() { return _instances.load(); }
    bool IsValid() const { return 0UL != _timerId; }
    // impl. of TranslatorEndPoint
    bool IsConnected() const final;
protected:
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const std::shared_ptr<Buffer>& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    static inline std::atomic<uint64_t> _instances = 0ULL;
    const bool _fileIsValid;
    const std::shared_ptr<TimerCallback> _callback;
    const std::shared_ptr<MediaTimer> _timer;
    const uint64_t _timerId;
    const uint32_t _intervalBetweenTranslationsMs;
    const uint32_t _connectionDelaylMs;
#ifdef MOCK_DISCONNECT_AFTER_MS
    uint64_t _disconnectedTimerId = 0U;
#endif
};

} // namespace RTC