#ifndef MS_RTC_MEDIA_FRAME_SERIALIZER_HPP
#define MS_RTC_MEDIA_FRAME_SERIALIZER_HPP

#include <memory>

namespace RTC
{

class RtpMediaFrame;
class OutputDevice;
class RtpCodecMimeType;

class RtpMediaFrameSerializer
{
public:
    virtual ~RtpMediaFrameSerializer() = default;
    virtual void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) = 0;
    virtual void SetOutputDevice(OutputDevice* outputDevice) { _outputDevice = outputDevice; }
    OutputDevice* GetOutputDevice() const { return _outputDevice; }
    static std::unique_ptr<RtpMediaFrameSerializer> create(const RtpCodecMimeType& mimeType);
protected:
    RtpMediaFrameSerializer() = default;
private:
    OutputDevice* _outputDevice = nullptr;
};

} // namespace RTC
#endif