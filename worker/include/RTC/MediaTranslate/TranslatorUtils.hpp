#pragma once

#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

class RtpStream;

std::string GetStreamInfoString(const RtpCodecMimeType& mime,
                                uint32_t mappedSsrc = 0U, uint32_t ssrc = 0U);
std::string GetStreamInfoString(uint32_t mappedSsrc, const RtpStream* stream);

const std::string& MimeTypeToString(RtpCodecMimeType::Type type);
const std::string& MimeTypeToString(const RtpCodecMimeType& mime);

const std::string& MimeSubTypeToString(RtpCodecMimeType::Subtype subtype);
const std::string& MimeSubTypeToString(const RtpCodecMimeType& mime);

} // namespace RTC
