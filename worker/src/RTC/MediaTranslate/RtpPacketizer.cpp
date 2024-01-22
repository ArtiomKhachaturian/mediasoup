#include "RTC/MediaTranslate/RtpPacketizer.hpp"
#include "DepLibUV.hpp"
#include "rtc_base/random.h"

namespace RTC
{

RtpPacketizer::RtpPacketizer()
{
    webrtc::Random random(DepLibUV::GetTimeMs());
    _mediaSequenceNumber = random.Rand(1, _maxInitRtpSeqNumber);
    _rtxSequenceNumber = random.Rand(1, _maxInitRtpSeqNumber);
}

} // namespace RTC
