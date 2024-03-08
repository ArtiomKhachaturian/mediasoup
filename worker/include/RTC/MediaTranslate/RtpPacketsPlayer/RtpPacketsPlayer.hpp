#pragma once
#include "RTC/Buffers/BufferAllocations.hpp"
#include "ProtectedObj.hpp"
#include <unordered_map>

namespace RTC
{

class RtpPacketsPlayerCallback;
class RtpPacketsInfoProvider;
class RtpCodecMimeType;
class MediaTimer;
class RtpPacketsPlayerStream;

class RtpPacketsPlayer : public BufferAllocations<void>
{
    using Streams = std::unordered_map<uint32_t, std::unique_ptr<RtpPacketsPlayerStream>>;
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
    ProtectedObj<Streams, std::mutex> _streams;
};

} // namespace RTC
