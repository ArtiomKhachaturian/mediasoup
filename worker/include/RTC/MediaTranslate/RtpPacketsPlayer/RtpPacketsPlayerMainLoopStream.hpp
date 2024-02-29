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
    static std::unique_ptr<RtpPacketsPlayerStream> Create(uint32_t ssrc, uint32_t clockRate,
                                                          uint8_t payloadType,
                                                          const RtpCodecMimeType& mime,
                                                          RtpPacketsPlayerCallback* callback,
                                                          const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    // impl. of RtpPacketsPlayerStream
    void Play(uint64_t mediaSourceId, const std::shared_ptr<Buffer>& media,
              const std::shared_ptr<MediaTimer> timer) final;
    void Stop(uint64_t mediaSourceId, uint64_t mediaId) final;
    bool IsPlaying() const final;
private:
    RtpPacketsPlayerMainLoopStream(std::unique_ptr<Impl> impl,
                                  std::unique_ptr<RtpPacketsPlayerStream> simpleStream);
private:
    const std::unique_ptr<Impl> _impl;
    const std::unique_ptr<RtpPacketsPlayerStream> _simpleStream;
};


} // namespace RTC