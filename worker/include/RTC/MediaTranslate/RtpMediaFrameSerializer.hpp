#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <string>

namespace RTC
{

class RtpMediaFrame;
class OutputDevice;
struct RtpAudioFrameConfig;
struct RtpVideoFrameConfig;

class RtpMediaFrameSerializer
{
public:
    RtpMediaFrameSerializer(const RtpMediaFrameSerializer&) = delete;
    RtpMediaFrameSerializer(RtpMediaFrameSerializer&&) = delete;
    virtual ~RtpMediaFrameSerializer() = default;
    virtual std::string_view GetFileExtension(const RtpCodecMimeType& mimeType) const;
    // both 'RegisterAudio' & 'RegisterVideo' should be called before 1st invoke of 'Push'
    virtual bool RegisterAudio(uint32_t ssrc,
                               RtpCodecMimeType::Subtype codec = RtpCodecMimeType::Subtype::UNSET,
                               const RtpAudioFrameConfig* config = nullptr) = 0;
    virtual bool RegisterVideo(uint32_t ssrc,
                               RtpCodecMimeType::Subtype codec = RtpCodecMimeType::Subtype::UNSET,
                               const RtpVideoFrameConfig* config = nullptr) = 0;
    virtual void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) = 0;
    virtual bool IsCompatible(const RtpCodecMimeType& mimeType) const = 0;
    virtual void SetOutputDevice(OutputDevice* outputDevice);
    virtual void SetLiveMode(bool /*liveMode*/ = true) {}
    OutputDevice* GetOutputDevice() const { return _outputDevice; }
    static std::shared_ptr<RtpMediaFrameSerializer> create(const RtpCodecMimeType& mimeType);
protected:
    RtpMediaFrameSerializer() = default;
private:
    OutputDevice* _outputDevice = nullptr;
};

} // namespace RTC
