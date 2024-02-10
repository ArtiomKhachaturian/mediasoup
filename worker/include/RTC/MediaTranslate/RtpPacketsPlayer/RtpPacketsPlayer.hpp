#pragma once
#include "ProtectedObj.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "absl/container/flat_hash_map.h"

namespace RTC
{

class RtpPacketsCollector;
class RtpPacketsInfoProvider;
class RtpCodecMimeType;

class RtpPacketsPlayer : public MediaSink
{
    class LoopWrapper;
    class TrackPlayer;
    class Stream;
public:
    RtpPacketsPlayer(RtpPacketsCollector* packetsCollector);
    ~RtpPacketsPlayer() final;
    void AddStream(uint32_t ssrc, const RtpCodecMimeType& mime,
                   const RtpPacketsInfoProvider* packetsInfoProvider);
    void RemoveStream(uint32_t ssrc);
    // impl. of MediaSink
    void StartMediaWriting(uint32_t ssrc) final;
    void WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting(uint32_t ssrc) final;
private:
    std::shared_ptr<Stream> GetStream(uint32_t ssrc) const;
private:
    RtpPacketsCollector* const _packetsCollector;
    MediaTimer _timer;
    ProtectedObj<absl::flat_hash_map<uint32_t, std::shared_ptr<Stream>>> _streams;
};

} // namespace RTC
