#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "ProtectedObj.hpp"
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
    MediaPacketsSink(std::unique_ptr<RtpMediaFrameSerializer> serializer);
    ~MediaPacketsSink() final;
    bool IsCompatible(const RtpCodecMimeType& mimeType) const;
    bool AddOutputDevice(OutputDevice* outputDevice);
    bool RemoveOutputDevice(OutputDevice* outputDevice);
    void AddPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet);
    //void Pause(bool pause = true) { _paused = pause; }
    //void Resume() { Pause(false); }
    //bool IsPaused() const { return _paused.load(std::memory_order_relaxed); }
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
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) final;
private:
    const std::unique_ptr<RtpMediaFrameSerializer> _serializer;
    ProtectedObj<OutputDevicesSet> _outputDevices;
    ProtectedObj<DepacketizersMap> _depacketizers;
    //std::atomic_bool _paused = false;
};

} // namespace RTC
