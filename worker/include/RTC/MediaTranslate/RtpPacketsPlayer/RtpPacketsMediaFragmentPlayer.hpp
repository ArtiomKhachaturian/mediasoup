#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "absl/container/flat_hash_map.h"

namespace RTC
{

class MemoryBuffer;
class MediaFrame;
class MediaFrameDeserializer;
class RtpCodecMimeType;
class RtpPacketizer;
class RtpPacketsPlayerCallback;
class RtpPacket;

class RtpPacketsMediaFragmentPlayer : public MediaTimerCallback
{
public:
    RtpPacketsMediaFragmentPlayer(const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                                  std::unique_ptr<MediaFrameDeserializer> deserializer,
                                  uint32_t ssrc, uint32_t clockRate, 
                                  uint8_t payloadType, uint64_t mediaId, uint64_t mediaSourceId);
    ~RtpPacketsMediaFragmentPlayer() final;
    uint64_t GetMediaId() const { return _mediaId; }
    bool Parse(const RtpCodecMimeType& mime, const std::shared_ptr<MemoryBuffer>& buffer);
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    void ConvertToRtpAndSend(size_t trackIndex,
                             const std::shared_ptr<RtpPacketsPlayerCallback>& callback,
                             const std::shared_ptr<const MediaFrame>& frame);
    RtpPacket* CreatePacket(size_t trackIndex, const std::shared_ptr<const MediaFrame>& frame) const;
private:
    const std::weak_ptr<RtpPacketsPlayerCallback> _playerCallbackRef;
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const uint8_t _payloadType;
    const uint64_t _mediaId;
    const uint64_t _mediaSourceId;
    // key is track number, value - packetizer instance
    absl::flat_hash_map<size_t, std::unique_ptr<RtpPacketizer>> _packetizers;
};

} // namespace RTC
