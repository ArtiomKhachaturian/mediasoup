#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaTimer;
class RtpPacketsPlayerStreamQueue;
class RtpPacketsPlayerCallback;

class RtpPacketsPlayerStream
{
public:
    RtpPacketsPlayerStream(uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
                           const RtpCodecMimeType& mime,
                           const std::shared_ptr<MediaTimer>& timer,
                           RtpPacketsPlayerCallback* callback);
    ~RtpPacketsPlayerStream();
    void Play(uint64_t mediaSourceId, const std::shared_ptr<MemoryBuffer>& media);
    bool IsPlaying() const;
    uint32_t GetSsrc() const { return _ssrc; }
private:
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const uint8_t _payloadType;
    const RtpCodecMimeType _mime;
    const std::shared_ptr<MediaTimer> _timer;
    const std::shared_ptr<RtpPacketsPlayerStreamQueue> _queue;
};

} // namespace RTC
