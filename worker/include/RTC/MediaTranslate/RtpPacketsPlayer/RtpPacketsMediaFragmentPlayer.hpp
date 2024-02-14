#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>
#include <atomic>

namespace RTC
{

class MemoryBuffer;
class MediaFrame;
class MediaTimer;
class MediaFrameDeserializer;
class RtpCodecMimeType;
class RtpPacketizer;
class RtpPacketsPlayerCallback;
class RtpPacket;

class RtpPacketsMediaFragmentPlayer : public MediaTimerCallback
{
    class PlayTask;
    class PlayTaskQueue;
public:
    RtpPacketsMediaFragmentPlayer(const std::weak_ptr<MediaTimer>& timerRef,
                                  const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                                  std::unique_ptr<MediaFrameDeserializer> deserializer,
                                  uint32_t ssrc, uint32_t clockRate, 
                                  uint8_t payloadType, uint64_t mediaId,
                                  const void* userData);
    ~RtpPacketsMediaFragmentPlayer() final;
    uint64_t SetTimerId(uint64_t timerId); // return previous ID
    uint64_t GetTimerId() const { return _timerId.load(); }
    uint64_t GetMediaId() const { return _mediaId; }
    bool Parse(const RtpCodecMimeType& mime, const std::shared_ptr<MemoryBuffer>& buffer);
    void PlayFrames();
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    void ConvertToRtpAndSend(size_t trackIndex,
                             const std::shared_ptr<RtpPacketsPlayerCallback>& callback,
                             const std::shared_ptr<const MediaFrame>& frame);
    RtpPacket* CreatePacket(size_t trackIndex, const std::shared_ptr<const MediaFrame>& frame) const;
private:
    const std::weak_ptr<MediaTimer> _timerRef;
    const std::weak_ptr<RtpPacketsPlayerCallback> _playerCallbackRef;
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    const uint32_t _ssrc;
    const uint32_t _clockRate;
    const uint8_t _payloadType;
    const uint64_t _mediaId;
    const void* const _userData;
    const std::unique_ptr<PlayTaskQueue> _tasksQueue;
    std::atomic<uint64_t> _timerId = 0ULL;
    uint32_t _previousTimestamp = 0U;
    // key is track number, value - packetizer instance
    ProtectedObj<absl::flat_hash_map<size_t, std::unique_ptr<RtpPacketizer>>> _packetizers;
};

} // namespace RTC
