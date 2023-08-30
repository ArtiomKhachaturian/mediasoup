#pragma once

#include <cstdint>

namespace RTC
{

class RtpCodecMimeType;

class OutputDevice
{
public:
    virtual ~OutputDevice() = default;
    virtual void BeginWriteMediaPayload(uint32_t /*ssrc*/, bool /*isKeyFrame*/,
                                        const RtpCodecMimeType& /*codecMimeType*/,
                                        uint16_t /*rtpSequenceNumber*/,
                                        uint32_t /*rtpTimestamp*/,
                                        uint32_t /*rtpAbsSendtime*/ = 0U,
                                        uint32_t /*duration*/ = 0U) {}
    virtual void EndWriteMediaPayload(uint32_t /*ssrc*/, bool /*ok*/) {}
    virtual bool Write(const void* buf, uint32_t len) = 0;
    virtual int64_t GetPosition() const = 0;
    // Set the current File position. Returns 0 on success.
    virtual bool SetPosition(int64_t /*position*/) { return false; }
    // Returns true if the device is seekable.
    virtual bool IsSeekable() const { return false; }
    virtual bool IsFileDevice() const { return false; }
};

} // namespace RTC
