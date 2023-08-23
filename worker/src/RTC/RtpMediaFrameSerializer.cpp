#define MS_CLASS "RTC::RtpMediaFrameSerializer"
#include "RTC/RtpMediaFrameSerializer.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/Codecs/RtpAudioWebMSerializer.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMediaFrameSerializer::RtpMediaFrameSerializer(OutputDevice* outputDevice)
	: _outputDevice(outputDevice)
{
    MS_ASSERT(nullptr != _outputDevice, "output device is null pointer");
}

std::unique_ptr<RtpMediaFrameSerializer> RtpMediaFrameSerializer::create(const RtpCodecMimeType& mimeType,
                                                                         OutputDevice* outputDevice)
{
    if (outputDevice) {
        switch (mimeType.type) {
            case RtpCodecMimeType::Type::AUDIO:
                switch (mimeType.subtype) {
                    case RtpCodecMimeType::Subtype::OPUS:
                        return std::make_unique<RtpAudioWebMSerializer>(outputDevice);
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    return nullptr;
}

} // namespace RTC
