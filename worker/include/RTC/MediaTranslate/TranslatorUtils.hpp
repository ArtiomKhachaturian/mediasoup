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

} // namespace RTC
