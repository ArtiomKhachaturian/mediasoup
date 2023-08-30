#pragma once

#include "ProtectedObj.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include <atomic>

namespace RTC
{

class RtpDepacketizer;
class RtpMediaFrameSerializer;
class RtpPacket;

class MediaPacketsSink : private OutputDevice
{
    using OutputDevicesSet = absl::flat_hash_set<OutputDevice*>;
    using DepacketizersMap = absl::flat_hash_map<RtpCodecMimeType::Subtype, std::shared_ptr<RtpDepacketizer>>;
public:
    MediaPacketsSink();
    ~MediaPacketsSink() final;
    bool AddOutputDevice(OutputDevice* outputDevice);
    bool RemoveOutputDevice(OutputDevice* outputDevice);
    void SetSerializer(std::unique_ptr<RtpMediaFrameSerializer> serializer);
    void AddPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet);
private:
    std::shared_ptr<RtpDepacketizer> FetchDepackizer(const RTC::RtpCodecMimeType& mimeType);
    // impl. of OutputDevice
    void BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                const RtpCodecMimeType& codecMimeType,
                                uint16_t rtpSequenceNumber,
                                uint32_t rtpTimestamp,
                                uint32_t rtpAbsSendtime,
                                uint32_t duration) final;
    void EndWriteMediaPayload(uint32_t ssrc, bool ok) final;
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _binaryWritePosition.load(std::memory_order_relaxed); }
private:
    ProtectedObj<OutputDevicesSet> _outputDevices;
    ProtectedObj<DepacketizersMap> _depacketizers;
    ProtectedUniquePtr<RtpMediaFrameSerializer> _serializer;
    std::atomic<int64_t> _binaryWritePosition = 0LL;
};

} // namespace RTC
