#pragma once
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "ProtectedObj.hpp"
#include "absl/container/flat_hash_map.h"
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaTimer;
class RtpPacketsPlayerMediaFragment;

class RtpPacketsPlayerSimpleStream : public RtpPacketsPlayerStream,
                                     private RtpPacketsPlayerCallback
{
    template<typename V>
    using UInt64Map = absl::flat_hash_map<uint64_t, V>;
    // key is media ID
    using MediaFragmentsMap = UInt64Map<std::shared_ptr<RtpPacketsPlayerMediaFragment>>;
    using MediaSourcesMap = UInt64Map<MediaFragmentsMap>;
public:
    ~RtpPacketsPlayerSimpleStream() final;
    static std::unique_ptr<RtpPacketsPlayerStream> Create(const std::shared_ptr<MediaTimer>& timer,
                                                          uint32_t ssrc, uint32_t clockRate,
                                                          uint8_t payloadType,
                                                          const RtpCodecMimeType& mime,
                                                          RtpPacketsPlayerCallback* callback);
    // impl. of RtpPacketsPlayerStream
    void Play(uint64_t mediaSourceId, const std::shared_ptr<MemoryBuffer>& media) final;
    bool IsPlaying() const final;
private:
    RtpPacketsPlayerSimpleStream(const std::shared_ptr<MediaTimer>& timer,
                                 uint32_t ssrc, uint32_t clockRate,
                                 uint8_t payloadType,
                                 const RtpCodecMimeType& mime,
                                 RtpPacketsPlayerCallback* callback);
    static bool IsPlaying(uint64_t mediaSourceId, uint64_t mediaId,
                          const MediaSourcesMap& playingMedias);
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
    void OnPlay(const Timestamp& timestampOffset, RtpPacket* packet, uint64_t mediaId,
                uint64_t mediaSourceId) final;
    void OnPlayFinished(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
private:
    const std::shared_ptr<MediaTimer> _timer;
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const uint8_t _payloadType;
    const RtpCodecMimeType _mime;
    RtpPacketsPlayerCallback* const _callback;
    // key is media source ID
    ProtectedObj<MediaSourcesMap> _playingMedias;
};

} // namespace RTC
