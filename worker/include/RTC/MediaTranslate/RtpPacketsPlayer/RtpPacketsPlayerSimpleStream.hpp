#pragma once
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaTimer;
class RtpCodecMimeType;
class RtpPacketsPlayerSimpleStreamQueue;
class RtpPacketsPlayerCallback;

class RtpPacketsPlayerSimpleStream : public RtpPacketsPlayerStream
{
public:
    ~RtpPacketsPlayerSimpleStream() final;
    static std::unique_ptr<RtpPacketsPlayerStream> Create(uint32_t ssrc, uint32_t clockRate,
                                                          uint8_t payloadType,
                                                          const RtpCodecMimeType& mime,
                                                          RtpPacketsPlayerCallback* callback);
    // impl. of RtpPacketsPlayerStream
    void Play(uint64_t mediaSourceId,
              const std::shared_ptr<MemoryBuffer>& media,
              const std::shared_ptr<MediaTimer>& timer) final;
    bool IsPlaying() const final;
private:
    RtpPacketsPlayerSimpleStream(std::shared_ptr<RtpPacketsPlayerSimpleStreamQueue> queue);
private:
    const std::shared_ptr<RtpPacketsPlayerSimpleStreamQueue> _queue;
};

} // namespace RTC
