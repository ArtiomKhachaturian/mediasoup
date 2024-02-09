#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include <mkvmuxer/mkvmuxer.h>

namespace RTC
{

bool WebMCodecs::IsSupported(const RtpCodecMimeType& mimeType)
{
    return nullptr != GetCodecId(mimeType);
}

const char* WebMCodecs::GetCodecId(RtpCodecMimeType::Subtype codec)
{
    // EMBL header for H264 & H265 will be 'matroska' and 'webm' for other codec types
    // https://www.matroska.org/technical/codec_specs.html
    switch (codec) {
        case RtpCodecMimeType::Subtype::VP8:
            return mkvmuxer::Tracks::kVp8CodecId;
        case RtpCodecMimeType::Subtype::VP9:
            return mkvmuxer::Tracks::kVp9CodecId;
        case RtpCodecMimeType::Subtype::H264:
        case RtpCodecMimeType::Subtype::H264_SVC:
            return "V_MPEG4/ISO/AVC"; // matroska
        case RtpCodecMimeType::Subtype::H265:
            return "V_MPEGH/ISO/HEVC";
        case RtpCodecMimeType::Subtype::PCMA:
        case RtpCodecMimeType::Subtype::PCMU:
            return "A_PCM/FLOAT/IEEE";
        case RtpCodecMimeType::Subtype::OPUS:
            return mkvmuxer::Tracks::kOpusCodecId;
        default:
            break;
    }
    return nullptr;
}

const char* WebMCodecs::GetCodecId(const RtpCodecMimeType& mime)
{
    return GetCodecId(mime.GetSubtype());
}

} // namespace RTC
