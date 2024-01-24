#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/RtpStream.hpp"
#include "DepLibUV.hpp"
#include "rtc_base/random.h"

namespace {

const std::string g_emptyString;

}

namespace RTC
{

std::string GetMediaFrameInfoString(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                    uint32_t mappedSsrc)
{
    if (mediaFrame) {
        auto streamInfo = GetStreamInfoString(mediaFrame->GetMimeType(), mappedSsrc);
        if (!streamInfo.empty()) {
            std::string configInfo;
            if (const auto config = mediaFrame->GetAudioConfig()) {
                configInfo = config->ToString();
            }
            else if (const auto config = mediaFrame->GetVideoConfig()) {
                configInfo = config->ToString();
            }
            if (!configInfo.empty()) {
                return streamInfo + ", " + configInfo;
            }
            return streamInfo;
        }
    }
    return std::string();
}

std::string GetStreamInfoString(const RtpCodecMimeType& mime, uint32_t mappedSsrc)
{
    if (mappedSsrc) {
        return mime.ToString() + ", mapped SSRC = " + std::to_string(mappedSsrc);
    }
    return std::string();
}

std::string GetStreamInfoString(uint32_t mappedSsrc, const RtpStream* stream)
{
    if (stream) {
        return GetStreamInfoString(stream->GetMimeType(), mappedSsrc);
    }
    return std::string();
}

const std::string& MimeTypeToString(RtpCodecMimeType::Type type)
{
    const auto it = RtpCodecMimeType::type2String.find(type);
    if (it != RtpCodecMimeType::type2String.end()) {
        return it->second;
    }
    return g_emptyString;
}

const std::string& MimeTypeToString(const RtpCodecMimeType& mime)
{
    if (mime.IsMediaCodec()) {
        return MimeTypeToString(mime.GetType());
    }
    return g_emptyString;
}

const std::string& MimeSubTypeToString(RtpCodecMimeType::Subtype subtype)
{
    const auto it = RtpCodecMimeType::subtype2String.find(subtype);
    if (it != RtpCodecMimeType::subtype2String.end()) {
        return it->second;
    }
    return g_emptyString;
}

const std::string& MimeSubTypeToString(const RtpCodecMimeType& mime)
{
    if (mime.IsMediaCodec()) {
        return MimeSubTypeToString(mime.GetSubtype());
    }
    return g_emptyString;
}

uint32_t GenerateRtpTimestamp()
{
    webrtc::Random random(DepLibUV::GetTimeUs());
    return random.Rand<uint32_t>();
}

uint16_t GenerateRtpInitialSequenceNumber(uint16_t maxInitRtpSeqNumber)
{
    webrtc::Random random(DepLibUV::GetTimeUs());
    return static_cast<uint16_t>(random.Rand(1u, uint32_t(maxInitRtpSeqNumber)));
}

} // namespace RTC
