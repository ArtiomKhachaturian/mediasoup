#pragma once
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerVpx : public RtpVideoDepacketizer
{
    class ShiftedPayload;
public:
    RtpDepacketizerVpx(uint32_t ssrc, uint32_t clockRate, bool vp8,
                       const std::shared_ptr<BufferAllocator>& allocator);
    ~RtpDepacketizerVpx() final;
    // impl. of RtpDepacketizer
    MediaFrame AddPacketInfo(uint32_t rtpTimestamp,
                             bool keyFrame, bool hasMarker,
                             const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                             const std::shared_ptr<Buffer>& payload,
                             bool* configWasChanged) final;
    VideoFrameConfig GetVideoConfig() const final { return _config; }
private:
    static bool ParseVp8VideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                    const std::shared_ptr<Buffer>& payload,
                                    VideoFrameConfig& applyTo);
    static bool ParseVp9VideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                    const std::shared_ptr<Buffer>& payload,
                                    VideoFrameConfig& applyTo);
    std::string GetDescription() const;
    MediaFrame MergePacketInfo(uint32_t rtpTimestamp,
                               bool keyFrame, bool hasMarker,
                               const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                               const std::shared_ptr<Buffer>& payload,
                               bool* configWasChanged);
    bool AddPayload(uint32_t rtpTimestamp, bool keyFrame,
                    const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                    const std::shared_ptr<Buffer>& payload);
    bool ParseVideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                          const std::shared_ptr<Buffer>& payload,
                          bool keyFrame, bool* configWasChanged);
    MediaFrame ResetFrame();
    void SetLastTimeStamp(uint32_t lastTimeStamp);
private:
    const bool _vp8;
    uint32_t _lastTimeStamp = 0U;
    MediaFrame _frame;
    VideoFrameConfig _config;
};

} // namespace RTC
