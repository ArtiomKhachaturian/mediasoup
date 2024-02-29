#ifndef MS_RTC_RTP_PACKET_HEADER_HPP
#define MS_RTC_RTP_PACKET_HEADER_HPP

#include "common.hpp"
#include <cstdint>

namespace RTC
{

/* Struct for RTP header. */
class RtpPacketHeader
{
public:
    // getters & setters
    uint8_t GetPayloadType() const { return this->payloadType; }
    void SetPayloadType(uint8_t payloadType) { this->payloadType = payloadType; }
    bool HasMarker() const { return this->marker; }
    void SetMarker(bool marker) { this->marker = marker; };
    bool HasPayloadPadding() const { return this->padding; }
    void SetPayloadPadding(bool padding) { this->padding = padding; }
    uint16_t GetSequenceNumber() const { return uint16_t{ ntohs(this->sequenceNumber) }; }
    void SetSequenceNumber(uint16_t seq) { this->sequenceNumber = uint16_t{ htons(seq) }; }
    uint32_t GetTimestamp() const { return uint32_t{ ntohl(this->timestamp) }; }
    void SetTimestamp(uint32_t timestamp) { this->timestamp = uint32_t{ htonl(timestamp) }; }
    uint32_t GetSsrc() const { return uint32_t{ ntohl(this->ssrc) }; }
    void SetSsrc(uint32_t ssrc) { this->ssrc = uint32_t{ htonl(ssrc) }; }
    bool HasExtension() const { return this->extension; }
    void SetExtension(bool extension) { this->extension = extension; }
    // getters only
    uint8_t GetCsrcCount() const { return this->csrcCount; }
    size_t GetCsrcListSize() const { return GetCsrcCount() * sizeof(this->ssrc); }
    uint8_t GetVersion() const { return this->version; }
    // helper routines
    static constexpr uint8_t GetDefaultVersion() { return 2; }
    static RtpPacketHeader* GetAsHeader(const uint8_t* data);
    static RtpPacketHeader* InitAsHeader(uint8_t* data);
    static bool IsRtp(const uint8_t* data, size_t len);
private:
    RtpPacketHeader() = default;
private:
#if defined(MS_LITTLE_ENDIAN)
    uint8_t csrcCount : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payloadType : 7;
    uint8_t marker : 1;
#elif defined(MS_BIG_ENDIAN)
    uint8_t version : 2;
    uint8_t padding : 1;
    uint8_t extension : 1;
    uint8_t csrcCount : 4;
    uint8_t marker : 1;
    uint8_t payloadType : 7;
#endif
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
};

static_assert(12U == sizeof(RtpPacketHeader), "wrong size of RtpPacketHeader");

} // namespace RTC

#endif
