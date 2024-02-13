#pragma once
#include "ProtectedObj.hpp"
#include "absl/container/flat_hash_map.h"

#define USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION

namespace RTC
{

class RtpPacketsPlayerCallback;
class RtpPacketsInfoProvider;
class RtpCodecMimeType;
class MediaTimer;
class MemoryBuffer;
class RtpPacketsPlayerStream;

class RtpPacketsPlayer
{
#ifdef USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION
    class StreamWrapper;
    using StreamType = StreamWrapper;
#else
    using StreamType = RtpPacketsPlayerStream;
#endif
public:
    RtpPacketsPlayer();
    ~RtpPacketsPlayer();
    void AddStream(uint32_t ssrc, const RtpCodecMimeType& mime,
                   RtpPacketsPlayerCallback* packetsCollector,
                   const RtpPacketsInfoProvider* packetsInfoProvider);
    void RemoveStream(uint32_t ssrc);
    bool IsPlaying(uint32_t ssrc) const;
    void Play(uint32_t ssrc, uint64_t mediaId, const std::shared_ptr<MemoryBuffer>& buffer,
              const void* userData = nullptr);
private:
    const std::shared_ptr<MediaTimer> _timer;
    ProtectedObj<absl::flat_hash_map<uint32_t, std::unique_ptr<StreamType>>> _streams;
};

} // namespace RTC
