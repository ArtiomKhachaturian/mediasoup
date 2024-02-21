#define MS_CLASS "RTC::TranslatorUtils"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include "RTC/RtpStream.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>

namespace {

const std::string g_emptyString;

}

namespace RTC
{

std::string GetMediaFrameInfoString(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                    uint32_t ssrc)
{
    if (mediaFrame) {
        auto streamInfo = GetStreamInfoString(mediaFrame->GetMimeType(), ssrc);
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

std::string GetCurrentTime()
{
    auto t = std::time(nullptr);
    auto tm = std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(tm, "%H-%M-%S");
    return oss.str();
}

std::string GetStreamInfoString(const RtpCodecMimeType& mime, uint32_t ssrc)
{
    if (ssrc) {
        return mime.ToString() + ", SSRC = " + std::to_string(ssrc);
    }
    return std::string();
}

std::string GetStreamInfoString(uint32_t ssrc, const RtpStream* stream)
{
    if (stream) {
        return GetStreamInfoString(stream->GetMimeType(), ssrc);
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

const char* ToString(MediaFrameDeserializeResult result)
{
    switch (result) {
        case MediaFrameDeserializeResult::ParseError:
            return "parse error";
        case MediaFrameDeserializeResult::OutOfMemory:
            return "out of memory";
        case MediaFrameDeserializeResult::InvalidArg:
            return "invalid argument";
        case MediaFrameDeserializeResult::Success:
            return "success";
        case MediaFrameDeserializeResult::NeedMoreData:
            return "need more data";
        default:
            break;
    }
    return "";
}

} // namespace RTC
