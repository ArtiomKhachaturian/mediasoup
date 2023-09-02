#define MS_CLASS "RTC::MediaPacketsSink"
#include "RTC/MediaTranslate/MediaPacketsSink.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaPacketsSink::MediaPacketsSink(std::unique_ptr<RtpMediaFrameSerializer> serializer)
    : _serializer(std::move(serializer))
{
    MS_ASSERT(_serializer, "serializer must not be null");
}

MediaPacketsSink::~MediaPacketsSink()
{
}

bool MediaPacketsSink::IsCompatible(const RtpCodecMimeType& mimeType) const
{
    return _serializer->IsCompatible(mimeType);
}

bool MediaPacketsSink::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        LOCK_WRITE_PROTECTED_OBJ(_outputDevices);
        if (!_outputDevices->count(outputDevice)) {
            _outputDevices->insert(outputDevice);
            if (1UL == _outputDevices->size()) {
                _serializer->SetOutputDevice(this);
            }
        }
        return true;
    }
    return false;
}

bool MediaPacketsSink::RemoveOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        LOCK_WRITE_PROTECTED_OBJ(_outputDevices);
        const auto it = _outputDevices->find(outputDevice);
        if (it != _outputDevices->end()) {
            _outputDevices->erase(it);
            if (_outputDevices->empty()) {
                _serializer->SetOutputDevice(nullptr);
            }
            return true;
        }
    }
    return false;
}

void MediaPacketsSink::AddPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet)
{
    if (packet && _serializer->GetOutputDevice()) {
        if (const auto depacketizer = FetchDepackizer(mimeType)) {
            _serializer->Push(depacketizer->AddPacket(packet));
        }
    }
}

std::shared_ptr<RtpDepacketizer> MediaPacketsSink::FetchDepackizer(const RTC::RtpCodecMimeType& mimeType)
{
    std::shared_ptr<RtpDepacketizer> depacketizer;
    LOCK_WRITE_PROTECTED_OBJ(_depacketizers);
    const auto it = _depacketizers->find(mimeType.subtype);
    if (it == _depacketizers->end()) {
        depacketizer = RtpDepacketizer::create(mimeType);
        if (!depacketizer) {
            MS_ERROR("failed create depacketizer for given MIME: %s", mimeType.ToString().c_str());
        }
        _depacketizers->insert({mimeType.subtype, depacketizer});
    }
    else {
        depacketizer = it->second;
    }
    return depacketizer;
}

void MediaPacketsSink::BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                              const RtpCodecMimeType& codecMimeType,
                                              uint16_t rtpSequenceNumber,
                                              uint32_t rtpTimestamp,
                                              uint32_t rtpAbsSendtime,
                                              uint32_t duration)
{
    LOCK_READ_PROTECTED_OBJ(_outputDevices);
    for (const auto outputDevice : _outputDevices.ConstRef()) {
        outputDevice->BeginWriteMediaPayload(ssrc, isKeyFrame, codecMimeType,
                                             rtpSequenceNumber, rtpTimestamp,
                                             rtpAbsSendtime, duration);
    }
}

void MediaPacketsSink::EndWriteMediaPayload(uint32_t ssrc, bool ok)
{
    LOCK_READ_PROTECTED_OBJ(_outputDevices);
    for (const auto outputDevice : _outputDevices.ConstRef()) {
        outputDevice->EndWriteMediaPayload(ssrc, ok);
    }
}

void MediaPacketsSink::Write(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (buffer) {
        LOCK_READ_PROTECTED_OBJ(_outputDevices);
        for (const auto outputDevice : _outputDevices.ConstRef()) {
            outputDevice->Write(buffer);
        }
    }
}

} // namespace RTC
