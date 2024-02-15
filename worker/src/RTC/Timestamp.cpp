#define MS_CLASS "RTC::Timestamp"
#include "RTC/Timestamp.hpp"
#include "Logger.hpp"

namespace {

inline constexpr uint64_t ValueToMicro(uint64_t value) {
    return value * 1000 * 1000;
}

template<typename T>
inline constexpr T ValueFromMicro(uint64_t micro) {
    return static_cast<T>(micro / 1000 / 1000);
}

}

namespace RTC
{

Timestamp::Timestamp(uint32_t clockRate)
    : _clockRate(clockRate)
{
    MS_ASSERT(_clockRate, "clock rate must be greater than zero");
}

Timestamp::Timestamp(uint32_t clockRate, uint32_t rtpTime)
    : Timestamp(clockRate)
{
    SetRtpTime(rtpTime);
}

Timestamp::Timestamp(uint32_t clockRate, const webrtc::Timestamp& time)
    : Timestamp(clockRate)
{
    SetTime(time);
}

webrtc::Timestamp Timestamp::GetTime() const
{
    return webrtc::Timestamp::us(ValueToMicro(GetRtpTime()) / GetClockRate());
}

void Timestamp::SetTime(const webrtc::Timestamp& time)
{
    SetRtpTime(ValueFromMicro<uint32_t>(time.us() * GetClockRate()));
}

bool Timestamp::operator == (const Timestamp& other) const
{
    return GetClockRate() == other.GetClockRate() && GetRtpTime() == other.GetRtpTime();
}

bool Timestamp::operator == (const webrtc::Timestamp& other) const
{
    return GetTime() == other;
}

bool Timestamp::operator != (const Timestamp& other) const
{
    return GetClockRate() != other.GetClockRate() || GetRtpTime() != other.GetRtpTime();
}

bool Timestamp::operator != (const webrtc::Timestamp& other) const
{
    return GetTime() != other;
}

bool Timestamp::operator <= (const Timestamp& other) const
{
    if (GetClockRate() != other.GetClockRate()) {
        return GetTime() <= other.GetTime();
    }
    return GetRtpTime() <= other.GetRtpTime();
}

bool Timestamp::operator <= (const webrtc::Timestamp& other) const
{
    return GetTime() <= other;
}

bool Timestamp::operator >= (const Timestamp& other) const
{
    if (GetClockRate() != other.GetClockRate()) {
        return GetTime() >= other.GetTime();
    }
    return GetRtpTime() >= other.GetRtpTime();
}

bool Timestamp::operator >= (const webrtc::Timestamp& other) const
{
    return GetTime() >= other;
}

bool Timestamp::operator > (const Timestamp& other) const
{
    if (GetClockRate() != other.GetClockRate()) {
        return GetTime() > other.GetTime();
    }
    return GetRtpTime() > other.GetRtpTime();
}

bool Timestamp::operator > (const webrtc::Timestamp& other) const
{
    return GetTime() > other;
}

bool Timestamp::operator < (const Timestamp& other) const
{
    if (GetClockRate() != other.GetClockRate()) {
        return GetTime() < other.GetTime();
    }
    return GetRtpTime() < other.GetRtpTime();
}

bool Timestamp::operator < (const webrtc::Timestamp& other) const
{
    return GetTime() < other;
}

webrtc::TimeDelta Timestamp::operator - (const Timestamp& other) const
{
    return GetTime() - other.GetTime();
}

webrtc::TimeDelta Timestamp::operator - (const webrtc::Timestamp& other) const
{
    return GetTime() - other;
}

} // namespace RTC
