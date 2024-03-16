#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"

namespace RTC
{

void VideoFrameConfig::SetWidth(int32_t width)
{
    _width = width;
}

void VideoFrameConfig::SetHeight(int32_t height)
{
    _height = height;
}

void VideoFrameConfig::SetFrameRate(double frameRate)
{
    _frameRate = frameRate;
}

bool VideoFrameConfig::operator == (const VideoFrameConfig& other) const
{
    return &other == this || (GetWidth() == other.GetWidth() &&
                              GetHeight() == other.GetHeight() &&
                              IsFloatsEqual(GetFrameRate(), other.GetFrameRate()) &&
                              IsCodecSpecificDataEqual(other));
}

bool VideoFrameConfig::operator != (const VideoFrameConfig& other) const
{
    if (&other != this) {
        return GetWidth() != other.GetWidth() ||
               GetHeight() != other.GetHeight() ||
               !IsFloatsEqual(GetFrameRate(), other.GetFrameRate()) ||
               !IsCodecSpecificDataEqual(other);
    }
    return false;
}

std::string VideoFrameConfig::ToString() const
{
    return std::to_string(GetWidth()) + "x" + std::to_string(GetHeight()) +
           " px, " + std::to_string(GetFrameRate()) + " fps";
}

} // namespace RTC
