#pragma once

#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

class RtpStream;

std::string GetStreamInfoString(uint32_t mappedSsrc, uint32_t ssrc,
                                const RtpCodecMimeType& mime);
std::string GetStreamInfoString(uint32_t mappedSsrc, const RtpStream* stream);

const std::string& MimeTypeToString(RtpCodecMimeType::Type type);
const std::string& MimeTypeToString(const RtpCodecMimeType& mime);

const std::string& MimeSubTypeToString(RtpCodecMimeType::Subtype subtype);
const std::string& MimeSubTypeToString(const RtpCodecMimeType& mime);

inline uint64_t MilliToNano(uint32_t milli) {
    return milli * 1000ULL * 1000ULL * 1000ULL;
}

} // namespace RTC
