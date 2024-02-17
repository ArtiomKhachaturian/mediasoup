#pragma once
#include "absl/container/flat_hash_map.h"
#include <atomic>
#include <memory>

namespace RTC
{

class MediaFrame;
class MemoryBuffer;
class MediaFrameDeserializer;
class RtpCodecMimeType;
class RtpPacketsPlayerCallback;
class RtpPacketizer;
class RtpPacket;

class RtpPacketsPlayerMediaFragment
{
public:
    RtpPacketsPlayerMediaFragment(std::unique_ptr<MediaFrameDeserializer> deserializer);
    ~RtpPacketsPlayerMediaFragment();
    bool Parse(uint32_t clockRate, const RtpCodecMimeType& mime, const std::shared_ptr<MemoryBuffer>& buffer);
    void Play(uint32_t ssrc, uint8_t payloadType, uint64_t mediaId,
              uint64_t mediaSourceId, RtpPacketsPlayerCallback* callback);
private:
    RtpPacket* CreatePacket(size_t trackIndex, uint32_t ssrc, uint8_t payloadType,
                            const std::shared_ptr<const MediaFrame>& frame) const;
private:
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    // key is track number, value - packetizer instance
    absl::flat_hash_map<size_t, std::unique_ptr<RtpPacketizer>> _packetizers;
};

} // namespace RTC
