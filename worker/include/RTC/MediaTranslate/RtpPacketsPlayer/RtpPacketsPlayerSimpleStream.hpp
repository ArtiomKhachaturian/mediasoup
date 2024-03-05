#pragma once
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "ProtectedObj.hpp"
#include <memory>
#include <unordered_map>

namespace RTC
{

class RtpPacketsPlayerMediaFragment;

class RtpPacketsPlayerSimpleStream : public BufferAllocations<RtpPacketsPlayerStream>,
                                     private RtpPacketsPlayerCallback
{
    template<typename V>
    using UInt64Map = std::unordered_map<uint64_t, V>;
    // key is media ID
    using MediaFragmentsMap = UInt64Map<std::unique_ptr<RtpPacketsPlayerMediaFragment>>;
    // key is media source ID
    using MediaSourcesMap = UInt64Map<MediaFragmentsMap>;
public:
    ~RtpPacketsPlayerSimpleStream() final;
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
    RtpPacketsPlayerSimpleStream(uint32_t ssrc, uint32_t clockRate,
                                 uint8_t payloadType,
                                 const RtpCodecMimeType& mime,
                                 RtpPacketsPlayerCallback* callback,
                                 const std::shared_ptr<BufferAllocator>& allocator);
    static bool IsPlaying(uint64_t mediaSourceId, uint64_t mediaId,
                          const MediaSourcesMap& playingMedias);
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc) final;
    void OnPlay(uint64_t mediaId, uint64_t mediaSourceId, RtpTranslatedPacket packet) final;
    void OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc) final;
private:
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const uint8_t _payloadType;
    const RtpCodecMimeType _mime;
    RtpPacketsPlayerCallback* const _callback;
    // key is media source ID
    ProtectedObj<MediaSourcesMap> _playingMedias;
};

} // namespace RTC
