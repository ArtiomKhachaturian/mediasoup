#pragma once

#include "RTC/MediaTranslate/ConsumerTranslatorSettings.hpp"

namespace RTC
{

class RtpPacketsCollector;
class Producer;
class RtpStream;

class ConsumerTranslator : public ConsumerTranslatorSettings
{
public:
    // return bind ID or zero (0) if failed
    virtual uint64_t Bind(uint32_t producerAudioSsrc, const std::string& producerId,
                          RtpPacketsCollector* output) = 0;
    uint64_t Bind(const RtpStream* producerAudioStream, const Producer* producerId,
                  RtpPacketsCollector* output);
    virtual void UnBind(uint64_t bindId) = 0;
};

} // namespace RTC
