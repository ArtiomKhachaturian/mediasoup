#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpStream.hpp"
#include "RTC/Producer.hpp"
#include "Logger.hpp"

namespace {

enum class MimeChangeStatus
{
    Changed,
    NotChanged,
    Failed
};

}

namespace RTC
{

class ProducerTranslator::StreamInfo : public RtpPacketsCollector
{
public:
    StreamInfo(uint32_t clockRate, uint32_t mappedSsrc);
    ~StreamInfo();
    uint32_t GetClockRate() const { return _clockRate; }
    uint32_t GetMappedSsrc() const { return _mappedSsrc; }
    MimeChangeStatus SetMime(const RtpCodecMimeType& mime);
    std::optional<RtpCodecMimeType> GetMime() const;
    std::string_view GetFileExtension() const;
    void SetSerializerOutputDevice(OutputDevice* outputDevice, bool set);
    // impl. of RtpPacketsCollector
    void AddPacket(const RtpPacket* packet) final;
private:
    const uint32_t _clockRate;
    const uint32_t _mappedSsrc;
    ProtectedUniquePtr<RtpDepacketizer> _depacketizer;
    ProtectedSharedPtr<RtpMediaFrameSerializer> _serializer;
};

ProducerTranslator::ProducerTranslator(Producer* producer)
    : _producer(producer)
{
    MS_ASSERT(_producer, "producer must not be null");
}

ProducerTranslator::~ProducerTranslator()
{
    for (const auto mappedSsrc : GetAddedStreams()) {
        RemoveStream(mappedSsrc);
    }
}

bool ProducerTranslator::IsAudio() const
{
    return Media::Kind::AUDIO == _producer->GetKind();
}

void ProducerTranslator::AddObserver(ProducerObserver* observer)
{
    _observers.Add(observer);
}

void ProducerTranslator::RemoveObserver(ProducerObserver* observer)
{
    _observers.Remove(observer);
}

bool ProducerTranslator::AddStream(const RtpStream* stream, uint32_t mappedSsrc)
{
    return mappedSsrc && stream && AddStream(stream->GetMimeType(),
                                             stream->GetClockRate(),
                                             mappedSsrc);
}

bool ProducerTranslator::AddStream(const RtpCodecMimeType& mime, uint32_t clockRate,
                                   uint32_t mappedSsrc)
{
    bool ok = false;
    if (mappedSsrc && mime.IsMediaCodec()) {
        MS_ASSERT(mime.IsAudioCodec() == IsAudio(), "mime types mistmatch");
        MS_ASSERT(clockRate, "clock rate must be greater than zero");
        const auto it = _streams.find(mappedSsrc);
        if (it == _streams.end()) {
            auto streamInfo = std::make_unique<StreamInfo>(clockRate, mappedSsrc);
            ok = MimeChangeStatus::Changed == streamInfo->SetMime(mime);
            if (ok) {
                _streams[mappedSsrc] = std::move(streamInfo);
                if (!_outputDevices.IsEmpty()) {
                    streamInfo->SetSerializerOutputDevice(this, true);
                }
                InvokeObserverMethod(&ProducerObserver::onStreamAdded, mappedSsrc,
                                     mime, clockRate);
            }
            else {
                const auto desc = GetStreamInfoString(mime, mappedSsrc);
                MS_ERROR("not found depacketizer or serializer for stream [%s]", desc.c_str());
            }
        }
        else {
            ok = false; // already registered
        }
    }
    return ok;
}

bool ProducerTranslator::RemoveStream(uint32_t mappedSsrc)
{
    if (mappedSsrc) {
        const auto it = _streams.find(mappedSsrc);
        if (it != _streams.end()) {
            const auto mime = it->second->GetMime();
            MS_ASSERT(mime.has_value(), "wrong stream without MIME info");
            it->second->SetSerializerOutputDevice(this, false);
            _streams.erase(it);
            InvokeObserverMethod(&ProducerObserver::onStreamRemoved, mappedSsrc, mime.value());
            return true;
        }
    }
    return false;
}

std::string_view ProducerTranslator::GetFileExtension(uint32_t mappedSsrc) const
{
    if (mappedSsrc) {
        const auto it = _streams.find(mappedSsrc);
        if (it != _streams.end()) {
            return it->second->GetFileExtension();
        }
    }
    return std::string_view();
}

std::list<uint32_t> ProducerTranslator::GetAddedStreams() const
{
    std::list<uint32_t> ssrcs;
    for (auto it = _streams.begin(); it != _streams.end(); ++it) {
        ssrcs.push_back(it->first);
    }
    return ssrcs;
}

const std::string& ProducerTranslator::GetId() const
{
    return _producer->id;
}

void ProducerTranslator::AddPacket(const RtpPacket* packet)
{
    if (packet && !IsPaused()) {
        const auto it = _streams.find(packet->GetSsrc());
        if (it != _streams.end()) {
            it->second->AddPacket(packet);
        }
    }
}

void ProducerTranslator::SetLanguage(const std::optional<MediaLanguage>& language)
{
    if (language != _language) {
        const auto from = _language;
        _language = language;
        InvokeObserverMethod(&ProducerObserver::OnLanguageChanged, from, language);
    }
}

void ProducerTranslator::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice && outputDevice != this && _outputDevices.Add(outputDevice) &&
        1UL == _outputDevices.GetSize()) {
        for (auto it = _streams.begin(); it != _streams.end(); ++it) {
            it->second->SetSerializerOutputDevice(this, true);
        }
    }
}

void ProducerTranslator::RemoveOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice && outputDevice != this && _outputDevices.Remove(outputDevice) &&
        _outputDevices.IsEmpty()) {
        for (auto it = _streams.begin(); it != _streams.end(); ++it) {
            it->second->SetSerializerOutputDevice(this, false);
        }
    }
}

template <class Method, typename... Args>
void ProducerTranslator::InvokeObserverMethod(const Method& method, Args&&... args) const
{
    _observers.InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

void ProducerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ProducerObserver::OnPauseChanged, pause);
}

void ProducerTranslator::StartStream(bool restart) noexcept
{
    _outputDevices.InvokeMethod(&OutputDevice::StartStream, restart);
}

void ProducerTranslator::BeginWriteMediaPayload(uint32_t ssrc,
                                                const std::vector<RtpMediaPacketInfo>& packets) noexcept
{
    _outputDevices.InvokeMethod(&OutputDevice::BeginWriteMediaPayload, ssrc, packets);
}

void ProducerTranslator::Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    _outputDevices.InvokeMethod(&OutputDevice::Write, buffer);
}

void ProducerTranslator::EndWriteMediaPayload(uint32_t ssrc, const std::vector<RtpMediaPacketInfo>& packets,
                                              bool ok) noexcept
{
    _outputDevices.InvokeMethod(&OutputDevice::EndWriteMediaPayload, ssrc, packets, ok);
}

void ProducerTranslator::EndStream(bool failure) noexcept
{
    _outputDevices.InvokeMethod(&OutputDevice::EndStream, failure);
}

ProducerTranslator::StreamInfo::StreamInfo(uint32_t clockRate, uint32_t mappedSsrc)
    : _clockRate(clockRate)
    , _mappedSsrc(mappedSsrc)
{
}

ProducerTranslator::StreamInfo::~StreamInfo()
{
}

MimeChangeStatus ProducerTranslator::StreamInfo::SetMime(const RtpCodecMimeType& mime)
{
    MimeChangeStatus status = MimeChangeStatus::Failed;
    LOCK_WRITE_PROTECTED_OBJ(_depacketizer);
    if (_depacketizer->get() && mime == _depacketizer->get()->GetMimeType()) {
        status = MimeChangeStatus::NotChanged;
    }
    else {
        if (auto depacketizer = RtpDepacketizer::create(mime, GetClockRate())) {
            if (auto serializer = RtpMediaFrameSerializer::create(mime)) {
                bool ok = false;
                switch (mime.GetType()) {
                    case RtpCodecMimeType::Type::AUDIO:
                        ok = serializer->AddAudio(GetMappedSsrc(), GetClockRate(), mime.GetSubtype());
                        break;
                    case RtpCodecMimeType::Type::VIDEO:
                        ok = serializer->AddVideo(GetMappedSsrc(), GetClockRate(), mime.GetSubtype());
                        break;
                    default:
                        break;
                }
                if (ok) {
                    LOCK_WRITE_PROTECTED_OBJ(_serializer);
                    _depacketizer = std::move(depacketizer);
                    _serializer = std::move(serializer);
                    status = MimeChangeStatus::Changed;
                }
                else {
                    // TODO: log error
                }
            }
            else {
                // TODO: log error
            }
        }
        else {
            // TODO: log error
        }
    }
    return status;
}

std::optional<RtpCodecMimeType> ProducerTranslator::StreamInfo::GetMime() const
{
    LOCK_READ_PROTECTED_OBJ(_depacketizer);
    if (const auto& depacketizer = _depacketizer.ConstRef()) {
        return depacketizer->GetMimeType();
    }
    return std::nullopt;
}

std::string_view ProducerTranslator::StreamInfo::GetFileExtension() const
{
    LOCK_READ_PROTECTED_OBJ(_serializer);
    if (const auto& serializer = _serializer.ConstRef()) {
        if (const auto mime = GetMime()) {
            return serializer->GetFileExtension(mime.value());
        }
    }
    return std::string_view();
}

void ProducerTranslator::StreamInfo::SetSerializerOutputDevice(OutputDevice* outputDevice, bool set)
{
    if (outputDevice) {
        LOCK_READ_PROTECTED_OBJ(_serializer);
        if (const auto& serializer = _serializer.ConstRef()) {
            if (set) {
                serializer->AddOutputDevice(outputDevice);
            }
            else {
                serializer->RemoveOutputDevice(outputDevice);
            }
        }
    }
}

void ProducerTranslator::StreamInfo::AddPacket(const RtpPacket* packet)
{
    if (packet) {
        MS_ASSERT(packet->GetSsrc() == GetMappedSsrc(), "invalid SSRC mapping");
        LOCK_READ_PROTECTED_OBJ(_depacketizer);
        if (const auto& depacketizer = _depacketizer.ConstRef()) {
            if (const auto frame = depacketizer->AddPacket(packet)) {
                LOCK_READ_PROTECTED_OBJ(_serializer);
                if (const auto& serializer = _serializer.ConstRef()) {
                    serializer->Push(frame);
                }
            }
        }
    }
}

} // namespace RTC
