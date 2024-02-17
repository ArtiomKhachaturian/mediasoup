#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaTimer;
class RtpCodecMimeType;
class RtpPacketsPlayerStreamQueue;
class RtpPacketsPlayerCallback;

class RtpPacketsPlayerStream
{
public:
    ~RtpPacketsPlayerStream();
    static std::unique_ptr<RtpPacketsPlayerStream> Create(uint32_t ssrc, uint32_t clockRate,
                                                          uint8_t payloadType,
                                                          const RtpCodecMimeType& mime,
                                                          RtpPacketsPlayerCallback* callback);
    void Play(uint64_t mediaSourceId, const std::shared_ptr<MemoryBuffer>& media,
              const std::shared_ptr<MediaTimer>& timer);
    bool IsPlaying() const;
    uint32_t GetSsrc() const;
private:
    RtpPacketsPlayerStream(std::shared_ptr<RtpPacketsPlayerStreamQueue> queue);
private:
    const std::shared_ptr<RtpPacketsPlayerStreamQueue> _queue;
};

} // namespace RTC
