#pragma once

#include "RTC/MediaTranslate/MediaFrameConfig.hpp"

namespace RTC
{

class VideoFrameConfig : public MediaFrameConfig
{
public:
    VideoFrameConfig() = default;
    void SetWidth(int32_t width);
    int32_t GetWidth() const { return _width; }
    void SetHeight(int32_t height);
    int32_t GetHeight() const { return _height; }
    void SetFrameRate(double frameRate); // optional
    double GetFrameRate() const { return _frameRate; }
    bool HasResolution() const { return GetWidth() > 0 && GetHeight() > 0; }
    // impl. of RtpMediaFrameConfig
    std::string ToString() const;
private:
    int32_t _width    = 0;
    int32_t _height   = 0;
    double _frameRate = 0.;
};

} // namespace RTC
