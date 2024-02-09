#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include <mkvmuxer/mkvmuxer.h>
#include <cstring>

namespace {

// emulation of non-standard [::strcmpi] function, return true if both strings are identical
inline bool CompareCaseInsensitive(const std::string_view& s1,
                                   const std::string_view& s2) {
    const size_t size = s1.size();
    if (size == s2.size()) {
        for (size_t i = 0UL; i < size; ++i) {
            if (std::tolower(s1[i]) != std::tolower(s2[i])) {
                return false;
            }
        }
        return true;
    }
    return false;
}

}

namespace RTC
{

bool WebMCodecs::IsSupported(const RtpCodecMimeType& mimeType)
{
    return nullptr != GetCodecId(mimeType);
}

bool WebMCodecs::IsSupported(const char* codecId)
{
    return codecId && IsSupported(std::string_view(codecId));
}

bool WebMCodecs::IsSupported(const std::string& codecId)
{
    if (!codecId.empty()) {
        return IsSupported(std::string_view(codecId.data(), codecId.size()));
    }
    return false;
}

bool WebMCodecs::IsSupported(const std::string_view& codecId)
{
    if (!codecId.empty()) {
        if (CompareCaseInsensitive(codecId, mkvmuxer::Tracks::kVp8CodecId)) {
            return true;
        }
        if (CompareCaseInsensitive(codecId, mkvmuxer::Tracks::kVp9CodecId)) {
            return true;
        }
        if (CompareCaseInsensitive(codecId, mkvmuxer::Tracks::kOpusCodecId)) {
            return true;
        }
        if (CompareCaseInsensitive(codecId, _h264CodecId)) {
            return true;
        }
        if (CompareCaseInsensitive(codecId, _h265CodecId)) {
            return true;
        }
        if (CompareCaseInsensitive(codecId, _pcmCodecId)) {
            return true;
        }
    }
    return false;
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
            return _h264CodecId;
        case RtpCodecMimeType::Subtype::H265:
            return _h265CodecId;
        case RtpCodecMimeType::Subtype::PCMA:
        case RtpCodecMimeType::Subtype::PCMU:
            return _pcmCodecId;
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
