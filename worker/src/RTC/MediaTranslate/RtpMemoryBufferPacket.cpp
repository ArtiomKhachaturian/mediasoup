#define MS_CLASS "RTC::RtpMemoryBufferPacket"
#include "RTC/MediaTranslate/RtpMemoryBufferPacket.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMemoryBufferPacket::RtpMemoryBufferPacket(std::unique_ptr<uint8_t[]> headerBuffer,
                                             const std::shared_ptr<const MemoryBuffer>& payload)
    : RtpPacket(reinterpret_cast<Header*>(headerBuffer.get()), nullptr,
                payload->GetData(), payload->GetSize(), 0U, payload->GetSize() + HeaderSize)
    , _headerBuffer(std::move(headerBuffer))
    , _payload(payload)
{
    const auto header = reinterpret_cast<Header*>(const_cast<uint8_t*>(_headerBuffer.get()));
    std::memset(header, 0, HeaderSize);
    header->version = Version; // default
}

RtpMemoryBufferPacket::~RtpMemoryBufferPacket()
{
}

RtpPacket* RtpMemoryBufferPacket::Create(const std::shared_ptr<const MemoryBuffer>& payload)
{
    RtpPacket* packet = nullptr;
    if (payload && !payload->IsEmpty()) {
        const auto payloadSize = payload->GetSize();
        if (payloadSize + HeaderSize > MtuSize) {
            MS_WARN_DEV_STD("size of memory buffer (%ld bytes) is greater than max MTU size", payloadSize);
        }
        std::unique_ptr<uint8_t[]> headerBuffer(new uint8_t[HeaderSize]);
        packet = new RtpMemoryBufferPacket(std::move(headerBuffer), payload);
        // workaround for fix issue with memory corruption in direct usage of RtpMemoryBufferPacket
        // TODO: solve this problem for production, maybe problem inside of RtpPacket::SetExtensions
        // because synthetic packet has no extensions and preallocated memory for that
        auto copy = packet->Clone();
        delete packet;
        packet = copy;
    }
    else {
        MS_WARN_DEV_STD("no payload memory buffer or empty");
    }
    return packet;
}

} // namespace RTC
