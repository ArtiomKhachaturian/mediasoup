#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_set.h>
#include <memory>
#include <string>

namespace RTC
{

class RtpMediaFrame;
class OutputDevice;
class MemoryBuffer;
class RtpAudioFrameConfig;
class RtpVideoFrameConfig;

class RtpMediaFrameSerializer : public ProducerInputMediaStreamer
{
    using OutputDevicesSet = absl::flat_hash_set<OutputDevice*>;
public:
    RtpMediaFrameSerializer(const RtpMediaFrameSerializer&) = delete;
    RtpMediaFrameSerializer(RtpMediaFrameSerializer&&) = delete;
    static std::shared_ptr<RtpMediaFrameSerializer> create(const RtpCodecMimeType& mimeType);
    virtual ~RtpMediaFrameSerializer() = default;
    virtual std::string_view GetFileExtension(const RtpCodecMimeType& mimeType) const;
    // both 'RegisterAudio' & 'RegisterVideo' maybe called before and affter 1st invoke of 'Push',
    // implementation should try to re-create media container if needed and send
    // restart event via 'OutputDevice::StartStream' (with restart=true)
    virtual bool AddAudio(uint32_t ssrc, uint32_t clockRate,
                          RtpCodecMimeType::Subtype codec = RtpCodecMimeType::Subtype::UNSET,
                          const std::shared_ptr<const RtpAudioFrameConfig>& config = nullptr) = 0;
    virtual bool AddVideo(uint32_t ssrc, uint32_t clockRate,
                          RtpCodecMimeType::Subtype codec = RtpCodecMimeType::Subtype::UNSET,
                          const std::shared_ptr<const RtpVideoFrameConfig>& config = nullptr) = 0;
    virtual void RemoveMedia(uint32_t ssrc) = 0;
    virtual void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) = 0;
    virtual bool IsCompatible(const RtpCodecMimeType& mimeType) const = 0;
    virtual void SetLiveMode(bool /*liveMode*/ = true) {}
    // impl. of ProducerInputMediaStreamer
    void AddOutputDevice(OutputDevice* outputDevice) final;
    void RemoveOutputDevice(OutputDevice* outputDevice) final;
protected:
    RtpMediaFrameSerializer() = default;
    virtual void onFirstOutputDeviceWasAdded() {}
    virtual void onLastOutputDeviceWillRemoved() {}
    bool HasDevices() const;
    void StartStream(bool restart);
    void BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                const RtpCodecMimeType& mimeType,
                                uint16_t rtpSequenceNumber,
                                uint32_t rtpTimestamp,
                                uint32_t rtpAbsSendtime);
    void WritePayload(const std::shared_ptr<const MemoryBuffer>& buffer);
    void EndWriteMediaPayload(uint32_t ssrc, bool ok);
    void EndStream(bool failure);
private:
    ProtectedObj<OutputDevicesSet> _outputDevices;
};

} // namespace RTC
