#ifndef MS_RTC_MEDIA_FRAME_SERIALIZER_HPP
#define MS_RTC_MEDIA_FRAME_SERIALIZER_HPP

#include <memory>

namespace RTC
{

class RtpMediaFrame;
class OutputDevice;

class RtpMediaFrameSerializer
{
public:
    virtual ~RtpMediaFrameSerializer() = default;
    virtual void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame) = 0;
protected:
    RtpMediaFrameSerializer(OutputDevice* outputDevice);
    OutputDevice* GetOutputDevice() const { return _outputDevice; }
private:
    OutputDevice* const _outputDevice;
};

} // namespace RTC
#endif
