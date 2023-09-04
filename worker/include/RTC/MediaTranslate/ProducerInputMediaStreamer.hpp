#pragma once

namespace RTC
{

class OutputDevice;

class ProducerInputMediaStreamer
{
public:
	virtual bool AddOutputDevice(OutputDevice* outputDevice) = 0;
    virtual bool RemoveOutputDevice(OutputDevice* outputDevice) = 0;
};

} // namespace RTC
