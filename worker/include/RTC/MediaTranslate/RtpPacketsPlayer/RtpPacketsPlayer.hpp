#pragma once
#include "ProtectedObj.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "absl/container/flat_hash_map.h"

namespace RTC
{

class RtpPacketsPlayerCallback;
class RtpPacketsInfoProvider;
class RtpCodecMimeType;
class MemoryBuffer;

class RtpPacketsPlayer
{
    //class TrackPlayer;
    //class Stream;
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
    //std::shared_ptr<Stream> GetStream(uint32_t ssrc) const;
private:
    MediaTimer _timer;
    //ProtectedObj<absl::flat_hash_map<uint32_t, std::shared_ptr<Stream>>> _streams;
};

} // namespace RTC
