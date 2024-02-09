#pragma once

#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

class WebMCodecs
{
public:
	static bool IsSupported(const RtpCodecMimeType& mimeType);
    // returns nullptr for unsupported mime/codec types
    static const char* GetCodecId(RtpCodecMimeType::Subtype codec);
    static const char* GetCodecId(const RtpCodecMimeType& mime);
};

} // namespace RTC
