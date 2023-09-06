#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpStream.hpp"

namespace {

const std::string g_emptyString;

}

namespace RTC
{

std::string GetStreamInfoString(const RtpCodecMimeType& mime,
                                uint32_t mappedSsrc, uint32_t ssrc)
{
    if (mappedSsrc || ssrc) {
        std::string ssrcInfo;
        if (ssrc) {
            ssrcInfo += ", SSRC = " + std::to_string(ssrc);
        }
        if (mappedSsrc) {
            ssrcInfo += ", mapped SSRC = " + std::to_string(mappedSsrc);
        }
        const std::string mimeString = mime.IsValid() ? mime.ToString() : "invalid mime";
        return "[" + mimeString + ssrcInfo + "]";
    }
    return std::string();
}

std::string GetStreamInfoString(uint32_t mappedSsrc, const RtpStream* stream)
{
    if (stream) {
        return GetStreamInfoString(stream->GetMimeType(), mappedSsrc, stream->GetSsrc());
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

} // namespace RTC
