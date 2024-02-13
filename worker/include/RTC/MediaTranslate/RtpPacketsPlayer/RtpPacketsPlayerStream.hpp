#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaTimer;
class RtpPacketsPlayerCallback;
class RtpPacketsInfoProvider;

class RtpPacketsPlayerStream
{
    class FragmentsQueue;
public:
    RtpPacketsPlayerStream(uint32_t ssrc, const RtpCodecMimeType& mime,
                           const std::shared_ptr<MediaTimer>& timer,
                           const RtpPacketsInfoProvider* packetsInfoProvider,
                           RtpPacketsPlayerCallback* callback);
    ~RtpPacketsPlayerStream();
    void Play(uint64_t mediaId, const std::shared_ptr<MemoryBuffer>& buffer,
              const void* userData = nullptr);
    bool IsPlaying() const;
private:
    const uint32_t _ssrc;
    const RtpCodecMimeType _mime;
    const std::shared_ptr<MediaTimer> _timer;
    const RtpPacketsInfoProvider* const _packetsInfoProvider;
    const std::shared_ptr<FragmentsQueue> _fragmentsQueue;
};

} // namespace RTC
