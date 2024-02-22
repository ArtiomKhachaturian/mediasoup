#pragma once
#include "RTC/MediaTranslate/Buffers/BufferAllocations.hpp"
#include "ProtectedObj.hpp"
#include "absl/container/flat_hash_map.h"

#define USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION

namespace RTC
{

class RtpPacketsPlayerCallback;
class RtpPacketsInfoProvider;
class RtpCodecMimeType;
class MediaTimer;
class RtpPacketsPlayerStream;

class RtpPacketsPlayer : public BufferAllocations<void>
{
public:
    RtpPacketsPlayer(const std::weak_ptr<BufferAllocator>& allocator);
    RtpPacketsPlayer(const std::shared_ptr<MediaTimer>& timer,
                     const std::weak_ptr<BufferAllocator>& allocator);
    ~RtpPacketsPlayer();
    void AddStream(uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
                   const RtpCodecMimeType& mime, RtpPacketsPlayerCallback* callback);
    void RemoveStream(uint32_t ssrc);
    bool IsPlaying(uint32_t ssrc) const;
    void Play(uint32_t ssrc, uint64_t mediaSourceId, const std::shared_ptr<Buffer>& media);
    const std::shared_ptr<MediaTimer>& GetTimer() const { return _timer; }
private:
    const std::shared_ptr<MediaTimer> _timer;
    ProtectedObj<absl::flat_hash_map<uint32_t, std::unique_ptr<RtpPacketsPlayerStream>>> _streams;
};

} // namespace RTC
