#pragma once
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "ProtectedObj.hpp"
#include <queue>

namespace RTC
{

class RtpPacketsPlayerMediaFragment;
class RtpPacketsPlayerCallback;
class MemoryBuffer;
class MediaTimer;

class RtpPacketsPlayerSimpleStreamQueue : public MediaTimerCallback,
                                          public std::enable_shared_from_this<RtpPacketsPlayerSimpleStreamQueue>
{
    using PendingMedia = std::pair<uint64_t, std::shared_ptr<MemoryBuffer>>;
public:
    ~RtpPacketsPlayerSimpleStreamQueue() final;
    static std::shared_ptr<RtpPacketsPlayerSimpleStreamQueue> Create(uint32_t ssrc, uint32_t clockRate,
                                                                     uint8_t payloadType,
                                                                     const RtpCodecMimeType& mime,
                                                                     RtpPacketsPlayerCallback* callback);
    void ResetCallback() { SetCallback(nullptr); }
    void Play(uint64_t mediaSourceId,
              const std::shared_ptr<MemoryBuffer>& media,
              const std::shared_ptr<MediaTimer>& timer);
    bool IsEmpty() const;
    uint32_t GetSsrc() const { return _ssrc; }
    uint32_t GetClockRate() const { return _clockRate; }
    uint8_t GetPayloadType() const { return _payloadType; }
    const RtpCodecMimeType& GetMime() const { return _mime; }
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    RtpPacketsPlayerSimpleStreamQueue(uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
                                      const RtpCodecMimeType& mime);
    void SetCallback(RtpPacketsPlayerCallback* callback);
private:
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const uint8_t _payloadType;
    const RtpCodecMimeType _mime;
    ProtectedObj<RtpPacketsPlayerCallback*> _callback = nullptr;
    ProtectedObj<std::queue<PendingMedia>> _pendingMedias;
};

} // namespace RTC
