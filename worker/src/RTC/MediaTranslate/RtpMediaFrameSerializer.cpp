#define MS_CLASS "RTC::RtpMediaFrameSerializer"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "Logger.hpp"

namespace RTC
{

void RtpMediaFrameSerializer::SetOutputDevice(OutputDevice* outputDevice)
{
    _outputDevice = outputDevice;
}

std::unique_ptr<RtpMediaFrameSerializer> RtpMediaFrameSerializer::create(const RtpCodecMimeType& mimeType)
{
    if (RtpWebMSerializer::IsSupported(mimeType)) {
        return std::make_unique<RtpWebMSerializer>();
    }
    return nullptr;
}

} // namespace RTC
