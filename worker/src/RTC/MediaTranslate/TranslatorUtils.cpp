#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpStream.hpp"

namespace {

const std::string g_emptyString;

}

namespace RTC
{

std::string GetStreamInfoString(uint32_t mappedSsrc, uint32_t ssrc,
                                const RtpCodecMimeType& mime)
{
    std::string mimeString;
    if (IsValidMediaMime(mime)) {
        mimeString = MimeTypeToString(mime.type) + "/" + MimeSubTypeToString(mime.subtype);
    }
    else {
        mimeString = "invalid mime";
    }
    return "[" + mimeString + ", SSRC = " + std::to_string(ssrc) + ", mapped SSRC = " +
           std::to_string(mappedSsrc) + "]";
}

std::string GetStreamInfoString(uint32_t mappedSsrc, const RtpStream* stream)
{
    if (stream) {
        return GetStreamInfoString(mappedSsrc, stream->GetSsrc(), stream->GetMimeType());
    }
    return std::string();
}

bool IsValidMediaMime(const RtpCodecMimeType& mime)
{
    return mime.IsMediaCodec() && (IsAudioMime(mime) || IsVideoMime(mime));
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
    if (IsVideoMime(mime)) {
        return MimeTypeToString(mime.type);
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
    if (IsVideoMime(mime)) {
        return MimeSubTypeToString(mime.subtype);
    }
    return g_emptyString;
}

} // namespace RTC
