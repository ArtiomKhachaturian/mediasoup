#pragma once

#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include <memory>

namespace RTC
{

class RtpPacketsCollector;
class RtpMediaFrameSerializer;
class RtpStream;

class ProducerTranslator : public ProducerInputMediaStreamer
{
public:
    virtual bool SetSerializer(uint32_t audioSsrc, std::unique_ptr<RtpMediaFrameSerializer> serializer) = 0;
    virtual RtpPacketsCollector* AddAudio(uint32_t audioSsrc) = 0;
    virtual RtpPacketsCollector* SetVideo(uint32_t videoSsrc, uint32_t associatedAudioSsrc) = 0;
    virtual bool RemoveAudio(uint32_t audioSsrc) = 0;
    virtual bool RemoveVideo(uint32_t videoSsrc) = 0;
    RtpPacketsCollector* SetVideo(const RtpStream* videoStream, const RtpStream* associatedAudioStream);
    RtpPacketsCollector* AddAudio(const RtpStream* audioStream);
    bool RemoveAudio(const RtpStream* audioStream);
    bool RemoveVideo(const RtpStream* videoStream);
    bool SetSerializer(const RtpStream* audioStream, std::unique_ptr<RtpMediaFrameSerializer> serializer);
};

} // namespace RTC
