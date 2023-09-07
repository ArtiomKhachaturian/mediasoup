#pragma once

namespace RTC
{

class OutputDevice;

class ProducerInputMediaStreamer
{
public:
	virtual void AddOutputDevice(OutputDevice* outputDevice) = 0;
    virtual void RemoveOutputDevice(OutputDevice* outputDevice) = 0;
};

} // namespace RTC
