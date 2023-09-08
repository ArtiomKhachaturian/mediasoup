#pragma once

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpDepacketizerVpx : public RtpDepacketizer
{
    class RtpAssembly;
public:
    RtpDepacketizerVpx(const RtpCodecMimeType& mimeType);
    ~RtpDepacketizerVpx() final;
    // impl. of RtpDepacketizer
    std::shared_ptr<const RtpMediaFrame> AddPacket(const RtpPacket* packet) final;
private:
    absl::flat_hash_map<uint32_t, std::unique_ptr<RtpAssembly>> _assemblies;
};

} // namespace RTC
