#define MS_CLASS "RTC::RtpMediaFrameSerializer"
#include "RTC/RtpMediaFrameSerializer.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMediaFrameSerializer::RtpMediaFrameSerializer(OutputDevice* outputDevice)
	: _outputDevice(outputDevice)
{
    MS_ASSERT(nullptr != _outputDevice, "output device is null pointer");
}

} // namespace RTC
