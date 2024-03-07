#pragma once
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStream.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerStreamCallback.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "ProtectedObj.hpp"
#include <memory>
#include <optional>
#include <unordered_map>

namespace RTC
{

class RtpPacketsPlayerMediaFragment;
class RtpPacketsPlayerCallback;

class RtpPacketsPlayerSimpleStream : public BufferAllocations<RtpPacketsPlayerStream>,
                                     private RtpPacketsPlayerStreamCallback
{
    template<typename V>
    using UInt64Map = std::unordered_map<uint64_t, V>;
    // 2nd is track number
    using PendingMedia = std::pair<std::unique_ptr<RtpPacketsPlayerMediaFragment>, size_t>;
    // key is media ID
    using PendingMediaMap = UInt64Map<PendingMedia>;
    // key is media source ID
    using PendingMediasMap = UInt64Map<PendingMediaMap>;
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
    void ActivateMedia(size_t trackIndex, std::unique_ptr<RtpPacketsPlayerMediaFragment> fragment);
    void ActivatePendingMedia(PendingMedia pendingMedia);
    void ActivateNextPendingMedia();
    bool DeactivateMedia(uint64_t mediaId, uint64_t mediaSourceId);
    void DeactivateMedia();
    std::optional<size_t> GetTrackIndex(const RtpPacketsPlayerMediaFragment* fragment) const;
    // impl. of RtpPacketsPlayerStreamCallback
    void OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId) final;
    void OnPlay(uint64_t mediaId, uint64_t mediaSourceId, RtpTranslatedPacket packet) final;
    void OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId) final;
private:
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const uint8_t _payloadType;
    const RtpCodecMimeType _mime;
    RtpPacketsPlayerCallback* const _callback;
    ProtectedUniquePtr<RtpPacketsPlayerMediaFragment> _activeMedia;
    ProtectedObj<PendingMediasMap> _pendingMedias;
};

} // namespace RTC
