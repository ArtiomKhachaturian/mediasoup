#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <list>

namespace RTC
{

class RtpMediaFrame;
class RtpMediaFrameSerializer;
class RtpPacket;

class RtpDepacketizer
{
public:
    virtual ~RtpDepacketizer() { DestroyPacketsChain(); }
    const RtpCodecMimeType& GetCodecMimeType() const { return _codecMimeType; }
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet);
    static std::unique_ptr<RtpDepacketizer> create(const RtpCodecMimeType& mimeType);
protected:
    RtpDepacketizer(const RtpCodecMimeType& codecMimeType);
    virtual std::shared_ptr<RtpMediaFrame> Assemble(const std::list<const RtpPacket*>& packets) const = 0;
private:
    void DestroyPacketsChain();
private:
    const RtpCodecMimeType _codecMimeType;
    std::list<const RtpPacket*> _packetsChain;
};

} // namespace RTC
