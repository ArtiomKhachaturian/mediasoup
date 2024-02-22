#pragma once
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"

namespace RTC
{

class BufferAllocator;
class MediaTimer;
class RtpCodecMimeType;
class RtpPacketsPlayerCallback;

class RtpPacketsPlayerMainLoopStream : public RtpPacketsPlayerStream
{
    class Impl;
public:
    ~RtpPacketsPlayerMainLoopStream() final;
    static std::unique_ptr<RtpPacketsPlayerStream> Create(const std::shared_ptr<MediaTimer>& timer,
                                                          uint32_t ssrc, uint32_t clockRate,
                                                          uint8_t payloadType,
                                                          const RtpCodecMimeType& mime,
                                                          RtpPacketsPlayerCallback* callback,
                                                          const std::weak_ptr<BufferAllocator>& allocator);
    // impl. of RtpPacketsPlayerStream
    void Play(uint64_t mediaSourceId, const std::shared_ptr<Buffer>& media) final;
    bool IsPlaying() const final;
private:
    RtpPacketsPlayerMainLoopStream(std::unique_ptr<Impl> impl,
                                  std::unique_ptr<RtpPacketsPlayerStream> simpleStream);
private:
    const std::unique_ptr<Impl> _impl;
    const std::unique_ptr<RtpPacketsPlayerStream> _simpleStream;
};


} // namespace RTC
