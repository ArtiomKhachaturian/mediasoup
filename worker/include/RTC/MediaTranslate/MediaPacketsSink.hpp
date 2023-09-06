#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include <string_view>

namespace RTC
{

class RtpMediaFrame;
class RtpCodecMimeType;

class MediaPacketsSink : public ProducerInputMediaStreamer, private OutputDevice
{
    class Serializer;
    using SerializersMap = absl::flat_hash_map<size_t, std::unique_ptr<Serializer>>;
    using OutputDevicesSet = absl::flat_hash_set<OutputDevice*>;
public:
    MediaPacketsSink();
    ~MediaPacketsSink() final;
    bool RegistertSerializer(const RtpCodecMimeType& mimeType);
    void UnRegisterSerializer(const RtpCodecMimeType& mimeType);
    void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame);
    std::string_view GetFileExtension(const RtpCodecMimeType& mime) const;
    // impl. of ProducerInputMediaStreamer
    bool AddOutputDevice(OutputDevice* outputDevice) final;
    bool RemoveOutputDevice(OutputDevice* outputDevice) final;
private:
    static size_t GetMimeKey(const RtpCodecMimeType& mimeType);
    void SetSerializersOutputDevice(OutputDevice* outputDevice) const;
    // impl. of OutputDevice
    void BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                const RtpCodecMimeType& mimeType,
                                uint16_t rtpSequenceNumber,
                                uint32_t rtpTimestamp,
                                uint32_t rtpAbsSendtime) final;
    void EndWriteMediaPayload(uint32_t ssrc, bool ok) final;
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) final;
private:
    ProtectedObj<SerializersMap> _serializers;
    ProtectedObj<OutputDevicesSet> _outputDevices;
};

} // namespace RTC
