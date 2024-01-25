#pragma once
#include <memory>

namespace RTC
{

class RtpPacket;
class MediaFrame;

class RtpPacketizer
{
public:
    virtual ~RtpPacketizer() = default;
    virtual RtpPacket* AddFrame(const std::shared_ptr<const MediaFrame>& frame) = 0;
protected:
    RtpPacketizer();
    //uint16_t GetNextSequenceNumber() { return _sequenceNumber++; }
private:
    // Random start, 16 bits. Upper half of range is avoided in order to prevent
    // wraparound issues during startup. Sequence number 0 is avoided for
    // historical reasons, presumably to avoid debugability or test usage
    // conflicts.
    static inline constexpr uint16_t _maxInitRtpSeqNumber = 0x7fff;  // 2^15 - 1.
    uint16_t _sequenceNumber;
};

} // namespace RTC
