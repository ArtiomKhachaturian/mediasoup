#pragma once

#include <memory>

namespace RTC
{

class RtpMediaFrame;
class OutputDevice;
class RtpCodecMimeType;

class RtpMediaFrameSerializer
{
public:
    RtpMediaFrameSerializer(const RtpMediaFrameSerializer&) = delete;
    RtpMediaFrameSerializer(RtpMediaFrameSerializer&&) = delete;
    virtual ~RtpMediaFrameSerializer() = default;
    virtual bool IsCompatible(const RtpCodecMimeType& mimeType) const = 0;
    virtual void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) = 0;
    virtual void SetOutputDevice(OutputDevice* outputDevice);
    virtual void SetLiveMode(bool /*liveMode*/ = true) {}
    OutputDevice* GetOutputDevice() const { return _outputDevice; }
    static std::unique_ptr<RtpMediaFrameSerializer> create(const RtpCodecMimeType& mimeType);
protected:
    RtpMediaFrameSerializer() = default;
private:
    OutputDevice* _outputDevice = nullptr;
};

} // namespace RTC
