#pragma once

#include "RTC/MediaTranslate/ProducerTranslatorSettings.hpp"

namespace RTC
{

class OutputDevice;

class ProducerInputMediaStreamer : public ProducerTranslatorSettings
{
public:
	virtual bool AddOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice) = 0;
    virtual bool RemoveOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice) = 0;
};

} // namespace RTC