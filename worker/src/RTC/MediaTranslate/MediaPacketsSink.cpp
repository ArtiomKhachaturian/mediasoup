#define MS_CLASS "RTC::MediaPacketsSink"
#include "RTC/MediaTranslate/MediaPacketsSink.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

namespace RTC
{

class MediaPacketsSink::Serializer
{
public:
    Serializer(const std::shared_ptr<RtpMediaFrameSerializer>& serializer);
    std::unique_ptr<Serializer> Clone() const;
    void Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame);
    std::string_view GetFileExtension(const RtpCodecMimeType& mime) const;
    void SetOutputDevice(OutputDevice* outputDevice);
    void SetLiveMode(bool liveMode);
    bool RegisterAudio(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                       const RtpAudioFrameConfig* config);
    bool RegisterVideo(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                       const RtpVideoFrameConfig* config);
    bool IsCompatible(const RtpCodecMimeType& mimeType) const;
    void IncRef();
    bool DecRef();
    // Returns true if has no more references
    bool HasOneRef() const;
private:
    const std::shared_ptr<RtpMediaFrameSerializer> _serializer;
    std::atomic<uint64_t> _refCounter = 1UL;
};

MediaPacketsSink::MediaPacketsSink()
{
}

MediaPacketsSink::~MediaPacketsSink()
{
    SetSerializersOutputDevice(nullptr);
}

void MediaPacketsSink::SetLiveMode(bool liveMode)
{
    LOCK_READ_PROTECTED_OBJ(_serializers);
    for (auto it = _serializers->begin(); it != _serializers->end(); ++it) {
        it->second->SetLiveMode(liveMode);
    }
}

bool MediaPacketsSink::RegisterSerializer(const RtpCodecMimeType& mime)
{
    bool ok = false;
    if (mime && mime.IsMediaCodec()) {
        const auto key = GetMimeKey(mime);
        LOCK_WRITE_PROTECTED_OBJ(_serializers);
        auto& serializers = _serializers.Ref();
        auto it = serializers.find(key);
        if (it == serializers.end()) {
            std::unique_ptr<Serializer> serializer;
            // search compatibly if any
            for (it = _serializers->begin(); it != _serializers->end(); ++it) {
                if (it->second->IsCompatible(mime)) {
                    serializer = it->second->Clone();
                    break;
                }
            }
            // compatible is not found
            if (!serializer) {
                if (const auto impl = RtpMediaFrameSerializer::create(mime)) {
                    serializer = std::make_unique<Serializer>(impl);
                    LOCK_READ_PROTECTED_OBJ(_outputDevices);
                    if (!_outputDevices->empty()) {
                        serializer->SetOutputDevice(this);
                    }
                }
                else {
                    // TODO: log error
                }
            }
            if (serializer) {
                serializers[key] = std::move(serializer);
                ok = true;
            }
        }
        else {
            it->second->IncRef();
            ok = true;
        }
    }
    return ok;
}

void MediaPacketsSink::UnRegisterSerializer(const RtpCodecMimeType& mime)
{
    if (mime && mime.IsMediaCodec()) {
        const auto key = GetMimeKey(mime);
        LOCK_WRITE_PROTECTED_OBJ(_serializers);
        const auto it = _serializers->find(key);
        if (it != _serializers->end() && it->second->DecRef()) {
            _serializers->erase(it);
        }
    }
}

bool MediaPacketsSink::RegisterAudio(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                                     const RtpAudioFrameConfig* config)
{
    if (ssrc && RtpCodecMimeType::Subtype::UNSET != codec) {
        const RtpCodecMimeType mime(RtpCodecMimeType::Type::AUDIO, codec);
        LOCK_READ_PROTECTED_OBJ(_serializers);
        const auto it = _serializers->find(GetMimeKey(mime));
        if (it != _serializers->end()) {
            return it->second->RegisterAudio(ssrc, codec, config);
        }
    }
    return false;
}

bool MediaPacketsSink::RegisterVideo(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                                     const RtpVideoFrameConfig* config)
{
    if (ssrc && RtpCodecMimeType::Subtype::UNSET != codec) {
        const RtpCodecMimeType mime(RtpCodecMimeType::Type::VIDEO, codec);
        LOCK_READ_PROTECTED_OBJ(_serializers);
        const auto it = _serializers->find(GetMimeKey(mime));
        if (it != _serializers->end()) {
            return it->second->RegisterVideo(ssrc, codec, config);
        }
    }
    return false;
}

void MediaPacketsSink::Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame) {
        LOCK_READ_PROTECTED_OBJ(_outputDevices);
        if (!_outputDevices->empty()) {
            const auto& mime = mediaFrame->GetCodecMimeType();
            LOCK_READ_PROTECTED_OBJ(_serializers);
            const auto it = _serializers->find(GetMimeKey(mime));
            if (it != _serializers->end()) {
                it->second->Push(mediaFrame);
            }
        }
    }
}

std::string_view MediaPacketsSink::GetFileExtension(const RtpCodecMimeType& mime) const
{
    if (mime && mime.IsMediaCodec()) {
        LOCK_READ_PROTECTED_OBJ(_serializers);
        const auto it = _serializers->find(GetMimeKey(mime));
        if (it != _serializers->end()) {
            return it->second->GetFileExtension(mime);
        }
    }
    return {};
}

bool MediaPacketsSink::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        LOCK_WRITE_PROTECTED_OBJ(_outputDevices);
        if (!_outputDevices->count(outputDevice)) {
            _outputDevices->insert(outputDevice);
            if (1UL == _outputDevices->size()) {
                SetSerializersOutputDevice(this);
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
                SetSerializersOutputDevice(nullptr);
            }
            return true;
        }
    }
    return false;
}

size_t MediaPacketsSink::GetMimeKey(const RtpCodecMimeType& mimeType)
{
    return Utils::HashCombine(mimeType.GetType(), mimeType.GetSubtype());
}

void MediaPacketsSink::SetSerializersOutputDevice(OutputDevice* outputDevice) const
{
    LOCK_READ_PROTECTED_OBJ(_serializers);
    for (auto it = _serializers->begin(); it != _serializers->end(); ++it) {
        it->second->SetOutputDevice(outputDevice);
    }
}

void MediaPacketsSink::BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                              const RtpCodecMimeType& codecMimeType,
                                              uint16_t rtpSequenceNumber,
                                              uint32_t rtpTimestamp,
                                              uint32_t rtpAbsSendtime)
{
    LOCK_READ_PROTECTED_OBJ(_outputDevices);
    for (const auto outputDevice : _outputDevices.ConstRef()) {
        outputDevice->BeginWriteMediaPayload(ssrc, isKeyFrame, codecMimeType,
                                             rtpSequenceNumber, rtpTimestamp,
                                             rtpAbsSendtime);
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

MediaPacketsSink::Serializer::Serializer(const std::shared_ptr<RtpMediaFrameSerializer>& serializer)
    : _serializer(serializer)
{
}

std::unique_ptr<MediaPacketsSink::Serializer> MediaPacketsSink::Serializer::Clone() const
{
    return std::make_unique<Serializer>(_serializer);
}

void MediaPacketsSink::Serializer::Push(const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    _serializer->Push(mediaFrame);
}

std::string_view MediaPacketsSink::Serializer::GetFileExtension(const RtpCodecMimeType& mime) const
{
    return _serializer->GetFileExtension(mime);
}

void MediaPacketsSink::Serializer::SetOutputDevice(OutputDevice* outputDevice)
{
    _serializer->SetOutputDevice(outputDevice);
}

void MediaPacketsSink::Serializer::SetLiveMode(bool liveMode)
{
    _serializer->SetLiveMode(liveMode);
}

bool MediaPacketsSink::Serializer::RegisterAudio(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                                                 const RtpAudioFrameConfig* config)
{
    return _serializer->RegisterAudio(ssrc, codec, config);
}

bool MediaPacketsSink::Serializer::RegisterVideo(uint32_t ssrc, RtpCodecMimeType::Subtype codec,
                                                 const RtpVideoFrameConfig* config)
{
    return _serializer->RegisterVideo(ssrc, codec, config);
}

bool MediaPacketsSink::Serializer::IsCompatible(const RtpCodecMimeType& mimeType) const
{
    return _serializer->IsCompatible(mimeType);
}

void MediaPacketsSink::Serializer::IncRef()
{
    // Relaxed memory order: The current thread is allowed to act on the
    // resource protected by the reference counter both before and after the
    // atomic op, so this function doesn't prevent memory access reordering.
    _refCounter.fetch_add(1U, std::memory_order_relaxed);
}

bool MediaPacketsSink::Serializer::DecRef()
{
    // Use release-acquire barrier to ensure all actions on the protected
    // resource are finished before the resource can be freed.
    // When refCountAfterSubtract > 0, this function require
    // std::memory_order_release part of the barrier.
    // When refCountAfterSubtract == 0, this function require
    // std::memory_order_acquire part of the barrier.
    // In addition std::memory_order_release is used for synchronization with
    // the HasOneRef function to make sure all actions on the protected resource
    // are finished before the resource is assumed to have exclusive access.
    const auto refCountAfterSubtract = _refCounter.fetch_sub(1U, std::memory_order_acq_rel) - 1U;
    return 0U == refCountAfterSubtract;
}

bool MediaPacketsSink::Serializer::HasOneRef() const
{
    // To ensure resource protected by the reference counter has exclusive
    // access, all changes to the resource before it was released by other
    // threads must be visible by current thread. That is provided by release
    // (in DecRef) and acquire (in this function) ordering.
    return _refCounter.load(std::memory_order_acquire) == 1ULL;
}

} // namespace RTC
