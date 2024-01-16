#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include "RTC/Listeners.hpp"
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
public:
    RtpMediaFrameSerializer(const RtpMediaFrameSerializer&) = delete;
    RtpMediaFrameSerializer(RtpMediaFrameSerializer&&) = delete;
    virtual ~RtpMediaFrameSerializer() = default;
    virtual std::string_view GetFileExtension(const RtpCodecMimeType& mimeType) const;
    // both 'RegisterAudio' & 'RegisterVideo' maybe called before and after 1st invoke of 'Push',
    // implementation should try to re-create media container if needed and send
    // restart event via 'OutputDevice::StartStream' (with restart=true)
    virtual bool AddAudio(uint32_t ssrc, uint32_t clockRate,
                          RtpCodecMimeType::Subtype codec,
                          const std::shared_ptr<const RtpAudioFrameConfig>& config = nullptr) = 0;
    virtual bool AddVideo(uint32_t ssrc, uint32_t clockRate,
                          RtpCodecMimeType::Subtype codec,
                          const std::shared_ptr<const RtpVideoFrameConfig>& config = nullptr) = 0;
    virtual void RemoveMedia(uint32_t ssrc) = 0;
    virtual bool Push(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) = 0;
    virtual bool IsCompatible(const RtpCodecMimeType& mimeType) const = 0;
    virtual void SetLiveMode(bool /*liveMode*/ = true) {}
    // impl. of ProducerInputMediaStreamer
    void AddOutputDevice(OutputDevice* outputDevice) final;
    void RemoveOutputDevice(OutputDevice* outputDevice) final;
protected:
    RtpMediaFrameSerializer() = default;
    bool HasDevices() const;
    void StartStream(bool restart) noexcept;
    void BeginWriteMediaPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) noexcept;
    void WritePayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept;
    void EndWriteMediaPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame,
                              bool ok) noexcept;
    void EndStream(bool failure) noexcept;
private:
    Listeners<OutputDevice*> _outputDevices;
};

} // namespace RTC
