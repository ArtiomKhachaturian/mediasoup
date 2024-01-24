#include "RTC/MediaTranslate/RtpPacketizer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"

namespace RTC
{

RtpPacketizer::RtpPacketizer()
    : _sequenceNumber(GenerateRtpInitialSequenceNumber(_maxInitRtpSeqNumber))
{
}

} // namespace RTC
