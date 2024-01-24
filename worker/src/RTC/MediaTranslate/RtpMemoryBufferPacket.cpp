#define MS_CLASS "RTC::RtpMemoryBufferPacket"
#include "RTC/MediaTranslate/RtpMemoryBufferPacket.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpMemoryBufferPacket::RtpMemoryBufferPacket(Header* header,
                                             HeaderExtension* headerExtension,
                                             const uint8_t* payload,
                                             size_t payloadLength,
                                             const std::shared_ptr<const MemoryBuffer>& buffer)
    : RtpPacket(header, headerExtension, payload, payloadLength, 0U, buffer ? buffer->GetSize() : 0U)
    , _buffer(buffer)
{
}

RtpMemoryBufferPacket::~RtpMemoryBufferPacket()
{
}

RtpPacket* RtpMemoryBufferPacket::Create(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    RtpPacket* packet = nullptr;
    if (buffer) {
        if (const auto data = buffer->GetData()) {
            if (const auto size = buffer->GetSize()) {
                if (size > HeaderSize) {
                    if (size > MtuSize) {
                        MS_WARN_TAG(rtp, "size of memory buffer (%ld bytes) is greater than max MTU size", size);
                    }
                    // format of minimal RTP header
                    // TODO: add header extension functionality
                    const auto header = reinterpret_cast<Header*>(const_cast<uint8_t*>(data));
                    std::memset(header, 0, HeaderSize);
                    header->version = Version; // default
                    packet = new RtpMemoryBufferPacket(header, nullptr, data + HeaderSize,
                                                       size - HeaderSize, buffer);
                }
                else {
                    MS_WARN_TAG(rtp, "size of memory buffer smaller that RTP header size");
                }
            }
            else {
                MS_WARN_TAG(rtp, "size of memory buffer is zero");
            }
        }
        else {
            MS_WARN_TAG(rtp, "memory buffer has no data");
        }
    }
    return packet;
}

} // namespace RTC
