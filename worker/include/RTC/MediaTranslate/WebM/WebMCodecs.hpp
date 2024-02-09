#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <string>

namespace RTC
{

class WebMCodecs
{
public:
	static bool IsSupported(const RtpCodecMimeType& mimeType);
    static bool IsSupported(const char* codecId);
    static bool IsSupported(const std::string_view& codecId);
    static bool IsSupported(const std::string& codecId);
    // returns nullptr for unsupported mime/codec types
    static const char* GetCodecId(RtpCodecMimeType::Subtype codec);
    static const char* GetCodecId(const RtpCodecMimeType& mime);
private:
    static inline const char* _h264CodecId = "V_MPEG4/ISO/AVC"; // matroshka
    static inline const char* _h265CodecId = "V_MPEGH/ISO/HEVC";
    static inline const char* _pcmCodecId  = "A_PCM/FLOAT/IEEE";
};

} // namespace RTC
