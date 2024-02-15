#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaTimer;
class MediaFrameDeserializer;
class RtpPacketsPlayerCallback;
class RtpPacketsMediaFragmentPlayer;

class RtpPacketsPlayerMediaFragment
{
public:
    RtpPacketsPlayerMediaFragment(const std::shared_ptr<MediaTimer>& timer,
                                  const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                                  std::unique_ptr<MediaFrameDeserializer> deserializer,
                                  uint32_t ssrc, uint32_t clockRate, uint8_t payloadType,
                                  uint64_t mediaId, uint64_t mediaSourceId);
    ~RtpPacketsPlayerMediaFragment();
    bool Parse(const RtpCodecMimeType& mime, const std::shared_ptr<MemoryBuffer>& buffer);
    void PlayFrames();
    bool IsPlaying() const;
    uint64_t GetMediaId() const;
private:
    const std::shared_ptr<RtpPacketsMediaFragmentPlayer> _player;
    const std::shared_ptr<MediaTimer> _timer;
};

} // namespace RTC
