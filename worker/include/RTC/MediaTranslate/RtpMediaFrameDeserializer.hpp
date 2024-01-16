#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <optional>

namespace RTC
{

class MemoryBuffer;

class RtpMediaFrameDeserializer
{
public:
    RtpMediaFrameDeserializer(const RtpMediaFrameDeserializer&) = delete;
    RtpMediaFrameDeserializer(RtpMediaFrameDeserializer&&) = delete;
    virtual ~RtpMediaFrameDeserializer() = default;
    virtual bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) = 0;
    // tracks info maybe actual after 1st calling of 'AddBuffer'
    virtual size_t GetTracksCount() const = 0;
    virtual std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackNumber) const = 0;

protected:
    RtpMediaFrameDeserializer() = default;
};

} // namespace RTC
