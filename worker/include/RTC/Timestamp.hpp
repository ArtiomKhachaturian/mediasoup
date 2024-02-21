#pragma once

#include "api/units/timestamp.h"

namespace RTC
{

// both kinds of time maybe absolute or relative
class Timestamp final
{
public:
    Timestamp() = delete;
    Timestamp(uint32_t clockRate);
    Timestamp(uint32_t clockRate, uint32_t rtpTime);
    Timestamp(uint32_t clockRate, const webrtc::Timestamp& time);
    Timestamp(const Timestamp&) = default;
    Timestamp(Timestamp&&) = default;
    uint32_t GetClockRate() const { return _clockRate; }
    uint32_t GetRtpTime() const { return _rtpTime; }
    void SetRtpTime(uint32_t rtpTime) { _rtpTime = rtpTime; }
    webrtc::Timestamp GetTime() const;
    void SetTime(const webrtc::Timestamp& time);
    bool IsZero() const { return 0U == GetRtpTime(); }
    // operators
    bool operator == (const Timestamp& other) const;
    bool operator == (const webrtc::Timestamp& other) const;
    bool operator != (const Timestamp& other) const;
    bool operator != (const webrtc::Timestamp& other) const;
    bool operator <= (const Timestamp& other) const;
    bool operator <= (const webrtc::Timestamp& other) const;
    bool operator >= (const Timestamp& other) const;
    bool operator >= (const webrtc::Timestamp& other) const;
    bool operator > (const Timestamp& other) const;
    bool operator > (const webrtc::Timestamp& other) const;
    bool operator < (const Timestamp& other) const;
    bool operator < (const webrtc::Timestamp& other) const;
    webrtc::TimeDelta operator - (const Timestamp& other) const;
    webrtc::TimeDelta operator - (const webrtc::Timestamp& other) const;
    operator webrtc::Timestamp () const { return GetTime(); }
    operator uint32_t () const { return GetRtpTime(); }
    Timestamp& operator = (const Timestamp&) = default;
    Timestamp& operator = (Timestamp&&) = default;
private:
    uint32_t _clockRate;
    uint32_t _rtpTime = 0U;
};

} // namespace RTC
