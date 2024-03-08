#pragma once
#include "RTC/Buffers/BufferAllocations.hpp"
#include "ProtectedObj.hpp"
#include "absl/container/flat_hash_map.h"

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
    RtpPacketsPlayer(const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~RtpPacketsPlayer();
    void AddStream(uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
                   const RtpCodecMimeType& mime, RtpPacketsPlayerCallback* callback);
    void RemoveStream(uint32_t ssrc);
    bool IsPlaying(uint32_t ssrc) const;
    void Play(uint32_t ssrc, uint64_t mediaSourceId, const std::shared_ptr<Buffer>& media);
    void Stop(uint32_t ssrc, uint64_t mediaSourceId, uint64_t mediaId = 0ULL);
    void Pause(uint32_t ssrc, bool pause);
    const std::shared_ptr<MediaTimer>& GetTimer() const { return _timer; }
private:
    const std::shared_ptr<MediaTimer> _timer;
    ProtectedObj<absl::flat_hash_map<uint32_t, std::unique_ptr<RtpPacketsPlayerStream>>> _streams;
};

} // namespace RTC
