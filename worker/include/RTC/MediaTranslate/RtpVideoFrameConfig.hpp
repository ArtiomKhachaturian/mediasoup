#pragma once

#include "RTC/MediaTranslate/RtpMediaFrameConfig.hpp"

namespace RTC
{

class MemoryBuffer;

class RtpVideoFrameConfig : public RtpMediaFrameConfig
{
public:
    RtpVideoFrameConfig() = default;
    void SetWidth(int32_t width);
    int32_t GetWidth() const { return _width.load(std::memory_order_relaxed); }
    void SetHeight(int32_t height);
    int32_t GetHeight() const { return _height.load(std::memory_order_relaxed); }
    void SetFrameRate(double frameRate); // optional
    double GetFrameRate() const { return _frameRate.load(std::memory_order_relaxed); }
    bool HasResolution() const { return GetWidth() > 0 && GetHeight() > 0; }
    bool ParseVp8VideoConfig(const RtpPacket* packet);
    bool ParseVp9VideoConfig(const RtpPacket* packet);
    // impl. of RtpMediaFrameConfig
    std::string ToString() const;
private:
    std::atomic<int32_t> _width    = 0;
    std::atomic<int32_t> _height   = 0;
    std::atomic<double> _frameRate = 0.;
    //std::atomic<const char*> _colourSpace = nullptr;
};

} // namespace RTC
