#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
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
    StreamInfo(uint32_t clockRate, uint32_t mappedSsrc, RtpMediaFrameSerializer* serializer);
    ~StreamInfo();
    uint32_t GetClockRate() const { return _clockRate; }
    uint32_t GetMappedSsrc() const { return _mappedSsrc; }
    MimeChangeStatus SetMime(const RtpCodecMimeType& mime);
    std::optional<RtpCodecMimeType> GetMime() const;
    std::string_view GetFileExtension() const;
    // impl. of RtpPacketsCollector
    bool AddPacket(RtpPacket* packet) final;
private:
    const uint32_t _clockRate;
    const uint32_t _mappedSsrc;
    RtpMediaFrameSerializer* const _serializer;
    ProtectedUniquePtr<RtpDepacketizer> _depacketizer;
};

ProducerTranslator::ProducerTranslator(Producer* producer,
                                       std::unique_ptr<RtpMediaFrameSerializer> serializer)
    : _producer(producer)
    , _serializer(std::move(serializer))
{
    MS_ASSERT(_producer, "producer must not be null");
    MS_ASSERT(_serializer, "media frame serializer must not be null");
    if (_producer->IsPaused()) {
        Pause();
    }
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
            if (_serializer->IsCompatible(mime)) {
                auto streamInfo = std::make_unique<StreamInfo>(clockRate, mappedSsrc, _serializer.get());
                ok = MimeChangeStatus::Changed == streamInfo->SetMime(mime);
                if (ok) {
                    _streams[mappedSsrc] = std::move(streamInfo);
                    InvokeObserverMethod(&ProducerObserver::onStreamAdded, mappedSsrc,
                                         mime, clockRate);
#ifdef WRITE_PRODUCER_RECV_TO_FILE
                    const auto it = _fileWriters.find(mappedSsrc);
                    if (it == _fileWriters.end()) {
                        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
                        if (depacketizerPath && std::strlen(depacketizerPath)) {
                            const auto extension = GetFileExtension(mappedSsrc);
                            if (!extension.empty()) {
                                std::string fileName = GetId() + "." + std::string(extension);
                                fileName = std::string(depacketizerPath) + "/" + fileName;
                                auto fileWriter = std::make_unique<FileWriter>(fileName);
                                if (fileWriter->IsOpen()) {
                                    AddOutputDevice(fileWriter.get());
                                    _fileWriters[mappedSsrc] = std::move(fileWriter);
                                }
                            }
                        }
                    }
#endif
                }
                else {
                    const auto desc = GetStreamInfoString(mime, mappedSsrc);
                    MS_ERROR("not found depacketizer for stream [%s]", desc.c_str());
                }
            }
            else {
                const auto desc = GetStreamInfoString(mime, mappedSsrc);
                MS_ERROR("stream [%s] is not compatible with target serializer", desc.c_str());
            }
        }
        else {
            ok = true; // already registered
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
            _streams.erase(it);
            InvokeObserverMethod(&ProducerObserver::onStreamRemoved, mappedSsrc, mime.value());
#ifdef WRITE_PRODUCER_RECV_TO_FILE
            const auto it = _fileWriters.find(mappedSsrc);
            if (it != _fileWriters.end()) {
                RemoveOutputDevice(it->second.get());
                _fileWriters.erase(it);
            }
#endif
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

bool ProducerTranslator::AddPacket(RtpPacket* packet)
{
    if (packet && !IsPaused()) {
        const auto it = _streams.find(packet->GetSsrc());
        if (it != _streams.end()) {
            return it->second->AddPacket(packet);
        }
    }
    return false;
}

std::optional<FBS::TranslationPack::Language> ProducerTranslator::GetLanguage() const
{
    return _producer->GetLanguage();
}

void ProducerTranslator::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice && outputDevice != this && _outputDevices.Add(outputDevice) &&
        1UL == _outputDevices.GetSize()) {
        _serializer->AddOutputDevice(this);
    }
}

void ProducerTranslator::RemoveOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice && outputDevice != this && _outputDevices.Remove(outputDevice) &&
        _outputDevices.IsEmpty()) {
        _serializer->RemoveOutputDevice(this);
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

ProducerTranslator::StreamInfo::StreamInfo(uint32_t clockRate, uint32_t mappedSsrc,
                                           RtpMediaFrameSerializer* serializer)
    : _clockRate(clockRate)
    , _mappedSsrc(mappedSsrc)
    , _serializer(serializer)
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
            bool ok = false;
            switch (mime.GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    ok = _serializer->AddAudio(GetMappedSsrc(), GetClockRate(), mime.GetSubtype());
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    ok = _serializer->AddVideo(GetMappedSsrc(), GetClockRate(), mime.GetSubtype());
                    break;
                default:
                    break;
            }
            if (ok) {
                _depacketizer = std::move(depacketizer);
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
    if (const auto mime = GetMime()) {
        return _serializer->GetFileExtension(mime.value());
    }
    return std::string_view();
}

bool ProducerTranslator::StreamInfo::AddPacket(RtpPacket* packet)
{
    if (packet) {
        MS_ASSERT(packet->GetSsrc() == GetMappedSsrc(), "invalid SSRC mapping");
        LOCK_READ_PROTECTED_OBJ(_depacketizer);
        if (const auto& depacketizer = _depacketizer.ConstRef()) {
            if (const auto frame = depacketizer->AddPacket(packet)) {
                return _serializer->Push(frame);
            }
        }
    }
    return false;
}

} // namespace RTC
