#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpDepacketizerVpx : public RtpDepacketizer
{
    class RtpAssembly;
public:
    RtpDepacketizerVpx(const RtpCodecMimeType& codecMimeType);
    ~RtpDepacketizerVpx() final;
    // impl. of RtpDepacketizer
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    std::allocator<uint8_t> _payloadAllocator;
    absl::flat_hash_map<uint32_t, std::unique_ptr<RtpAssembly>> _assemblies;
};

} // namespace RTC
