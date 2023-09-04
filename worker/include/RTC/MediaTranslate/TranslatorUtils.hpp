#pragma once

#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

class RtpStream;

std::string GetStreamInfoString(uint32_t mappedSsrc, uint32_t ssrc,
                                const RtpCodecMimeType& mime);
std::string GetStreamInfoString(uint32_t mappedSsrc, const RtpStream* stream);

inline bool IsAudioMime(const RtpCodecMimeType& mime) {
    return RtpCodecMimeType::Type::AUDIO == mime.type;
}

inline bool IsVideoMime(const RtpCodecMimeType& mime) {
    return RtpCodecMimeType::Type::VIDEO == mime.type;
}

bool IsValidMediaMime(const RtpCodecMimeType& mime);

const std::string& MimeTypeToString(RtpCodecMimeType::Type type);
const std::string& MimeTypeToString(const RtpCodecMimeType& mime);

const std::string& MimeSubTypeToString(RtpCodecMimeType::Subtype subtype);
const std::string& MimeSubTypeToString(const RtpCodecMimeType& mime);

} // namespace RTC
