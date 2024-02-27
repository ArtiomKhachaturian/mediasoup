#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <cmath>

namespace RTC
{

// MIME helpers
class RtpStream;

inline const char* GetAgentName() { return "SpeakShiftSFU"; }

template <typename TFloat1, typename TFloat2>
inline bool IsFloatsEqual(TFloat1 x, TFloat2 y)
{
    static_assert(std::is_arithmetic<TFloat1>::value && std::is_arithmetic<TFloat2>::value);
    bool const xnan = std::isnan(x), ynan = std::isnan(y);
    if (xnan || ynan) { // if one is nan -> return false, if both -> true
        return xnan && ynan;
    }
    // both values should not be greater or less of each other
    return !std::islessgreater(x, y);
}

std::string GetCurrentTime();

std::string GetStreamInfoString(const RtpCodecMimeType& mime, uint32_t ssrc = 0U);
std::string GetStreamInfoString(uint32_t ssrc, const RtpStream* stream);

const std::string& MimeTypeToString(RtpCodecMimeType::Type type);
const std::string& MimeTypeToString(const RtpCodecMimeType& mime);

const std::string& MimeSubTypeToString(RtpCodecMimeType::Subtype subtype);
const std::string& MimeSubTypeToString(const RtpCodecMimeType& mime);


} // namespace RTC
