#define MS_CLASS "RTC::MediaPacketsSink"
#include "RTC/MediaTranslate/Details/MediaPacketsSink.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaPacketsSink::MediaPacketsSink()
{
}

MediaPacketsSink::~MediaPacketsSink()
{
}

bool MediaPacketsSink::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        LOCK_WRITE_PROTECTED_OBJ(_outputDevices);
        if (!_outputDevices->count(outputDevice)) {
            _outputDevices->insert(outputDevice);
            if (1UL == _outputDevices->size()) {
                LOCK_READ_PROTECTED_OBJ(_serializer);
                if (const auto& serializer = _serializer.constRef()) {
                    serializer->SetOutputDevice(this);
                }
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
                LOCK_READ_PROTECTED_OBJ(_serializer);
                if (const auto& serializer = _serializer.constRef()) {
                    serializer->SetOutputDevice(nullptr);
                }
            }
            return true;
        }
    }
    return false;
}

void MediaPacketsSink::SetSerializer(std::unique_ptr<RtpMediaFrameSerializer> serializer)
{
    LOCK_WRITE_PROTECTED_OBJ(_serializer);
    if (serializer != _serializer.constRef()) {
        if (const auto& serializer = _serializer.constRef()) {
            serializer->SetOutputDevice(nullptr);
        }
        _serializer = std::move(serializer);
        if (const auto& serializer = _serializer.constRef()) {
            LOCK_READ_PROTECTED_OBJ(_outputDevices);
            if (!_outputDevices->empty()) {
                serializer->SetOutputDevice(this);
            }
        }
    }
}

void MediaPacketsSink::AddPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet)
{
    if (packet) {
        LOCK_READ_PROTECTED_OBJ(_serializer);
        const auto& serializer = _serializer.constRef();
        if (serializer && serializer->GetOutputDevice()) {
            if (const auto depacketizer = FetchDepackizer(mimeType)) {
                serializer->Push(depacketizer->AddPacket(packet));
            }
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
            // TODO: add mime description to error
            MS_ERROR("Failed create depacketizer for given MIME");
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
    for (const auto outputDevice : _outputDevices.constRef()) {
        outputDevice->BeginWriteMediaPayload(ssrc, isKeyFrame, codecMimeType,
                                             rtpSequenceNumber, rtpTimestamp,
                                             rtpAbsSendtime, duration);
    }
}

void MediaPacketsSink::EndWriteMediaPayload(uint32_t ssrc, bool ok)
{
    LOCK_READ_PROTECTED_OBJ(_outputDevices);
    for (const auto outputDevice : _outputDevices.constRef()) {
        outputDevice->EndWriteMediaPayload(ssrc, ok);
    }
}

bool MediaPacketsSink::Write(const void* buf, uint32_t len)
{
    bool ok = false;
    if (buf && len) {
        size_t failures = 0UL;
        {
            LOCK_READ_PROTECTED_OBJ(_outputDevices);
            for (const auto outputDevice : _outputDevices.constRef()) {
                if (!outputDevice->Write(buf, len)) {
                    ++failures;
                }
            }
            ok = failures < _outputDevices->size();
        }
        if (ok) {
            _binaryWritePosition.fetch_add(len);
        }
    }
    return ok;
}

} // namespace RTC
