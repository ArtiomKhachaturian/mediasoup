#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#include "RTC/RtpStream.hpp"
#include "RTC/Producer.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include <absl/container/flat_hash_set.h>

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

class ProducerTranslator::StreamInfo : public ProducerInputMediaStreamer,
                                       private OutputDevice
{
    using OutputDevicesSet = absl::flat_hash_set<OutputDevice*>;
public:
    StreamInfo(uint32_t sampleRate, uint32_t mappedSsrc, uint32_t ssrc);
    ~StreamInfo() final;
    uint32_t GetMappedSsrc() const { return _mappedSsrc; }
    void SetSsrc(uint32_t ssrc) { _ssrc = ssrc; }
    MimeChangeStatus SetMime(const RtpCodecMimeType& mime);
    std::optional<RtpCodecMimeType> GetMime() const;
    void DepacketizeAndSerialize(const RtpPacket* packet) const;
    // impl. of ProducerInputMediaStreamer
    uint32_t GetSsrc() const final { return _ssrc.load(std::memory_order_relaxed); }
    bool AddOutputDevice(OutputDevice* outputDevice) final;
    bool RemoveOutputDevice(OutputDevice* outputDevice) final;
private:
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    std::string_view GetFileExtension(const RtpCodecMimeType& mime) const;
    bool EnsureFileWriter(const RTC::RtpCodecMimeType& mime);
#endif
    void SetOutputDevice(OutputDevice* outputDevice) const;
    void SetLiveMode(bool live) const;
    // impl. of OutputDevice
    void BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                const RtpCodecMimeType& mimeType,
                                uint16_t rtpSequenceNumber,
                                uint32_t rtpTimestamp,
                                uint32_t rtpAbsSendtime) final;
    void EndWriteMediaPayload(uint32_t ssrc, bool ok) final;
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) final;
private:
    const uint32_t _sampleRate;
    const uint32_t _mappedSsrc;
    std::atomic<uint32_t> _ssrc;
    ProtectedUniquePtr<RtpDepacketizer> _depacketizer;
    ProtectedUniquePtr<RtpMediaFrameSerializer> _serializer;
    ProtectedObj<OutputDevicesSet> _outputDevices;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    ProtectedUniquePtr<FileWriter> _fileWriter;
#endif
};

ProducerTranslator::ProducerTranslator(Producer* producer)
    : _producer(producer)
{
    MS_ASSERT(_producer, "producer must not be null");
}

ProducerTranslator::~ProducerTranslator()
{
    for (const auto mappedSsrc : GetRegisteredSsrcs(true)) {
        UnRegisterStream(mappedSsrc);
    }
}

bool ProducerTranslator::IsAudio() const
{
    return Media::Kind::AUDIO == _producer->GetKind();
}

void ProducerTranslator::AddObserver(ProducerObserver* observer)
{
    if (observer) {
        if (_observers.end() == std::find(_observers.begin(), _observers.end(), observer)) {
            _observers.push_back(observer);
        }
    }
}

void ProducerTranslator::RemoveObserver(ProducerObserver* observer)
{
    if (observer) {
        const auto it = std::find(_observers.begin(), _observers.end(), observer);
        if (it != _observers.end()) {
            _observers.erase(it);
        }
    }
}

bool ProducerTranslator::RegisterStream(const RtpStream* stream, uint32_t mappedSsrc)
{
    bool ok = false;
    if (mappedSsrc && stream) {
        const auto& mime = stream->GetMimeType();
        MS_ASSERT(mime.IsAudioCodec() == IsAudio(), "mime types mistmatch");
        if (mime.IsMediaCodec()) {
            const auto it = _streams.find(mappedSsrc);
            if (it == _streams.end()) {
                const auto streamInfo = std::make_shared<StreamInfo>(stream->GetClockRate(),
                                                                     mappedSsrc,
                                                                     stream->GetSsrc());
                ok = MimeChangeStatus::Changed == streamInfo->SetMime(mime);
                if (ok) {
                    _streams[mappedSsrc] = streamInfo;
                    onProducerStreamRegistered(streamInfo, mappedSsrc, true);
                }
                else {
                    const auto desc = GetStreamInfoString(mappedSsrc, stream);
                    MS_ERROR("not found depacketizer or serializer for stream [%s]", desc.c_str());
                }
            }
            else {
                const auto& streamInfo = it->second;
                const auto ssrcChanged = streamInfo->GetSsrc() != stream->GetSsrc();
                if (ssrcChanged) {
                    onProducerStreamRegistered(streamInfo, mappedSsrc, false);
                    streamInfo->SetSsrc(stream->GetSsrc());
                }
                const auto oldMime = streamInfo->GetMime();
                if (oldMime != mime) {
                    ok = MimeChangeStatus::Failed != streamInfo->SetMime(mime);
                    if (!ok) {
                        const auto desc = GetStreamInfoString(mappedSsrc, stream);
                        MS_ERROR("no depacketizer found for stream [%s]", desc.c_str());
                    }
                }
                else {
                    ok = true;
                }
                if (ok) {
                    if (ssrcChanged) {
                        onProducerStreamRegistered(streamInfo, mappedSsrc, true);
                    }
                }
                else {
                    _streams.erase(it);
                }
            }
        }
    }
    return ok;
}

bool ProducerTranslator::UnRegisterStream(uint32_t mappedSsrc)
{
    if (mappedSsrc) {
        const auto it = _streams.find(mappedSsrc);
        if (it != _streams.end()) {
            const auto streamInfo = it->second;
            _streams.erase(it);
            onProducerStreamRegistered(streamInfo, mappedSsrc, false);
            return true;
        }
    }
    return false;
}

std::shared_ptr<ProducerInputMediaStreamer> ProducerTranslator::GetMediaStreamer(uint32_t mappedSsrc) const
{
    const auto it = _streams.find(mappedSsrc);
    if (it != _streams.end()) {
        return it->second;
    }
    return nullptr;
}

std::list<uint32_t> ProducerTranslator::GetRegisteredSsrcs(bool mapped) const
{
    std::list<uint32_t> ssrcs;
    for (auto it = _streams.begin(); it != _streams.end(); ++it) {
        if (mapped) {
            ssrcs.push_back(it->first);
        }
        else {
            ssrcs.push_back(it->second->GetSsrc());
        }
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
            it->second->DepacketizeAndSerialize(packet);
        }
    }
}

void ProducerTranslator::SetLanguage(const std::optional<MediaLanguage>& language)
{
    if (language != _language) {
        const auto from = _language;
        _language = language;
        InvokeObserverMethod(&ProducerObserver::OnProducerLanguageChanged, from, language);
    }
}

template <class Method, typename... Args>
void ProducerTranslator::InvokeObserverMethod(const Method& method, Args&&... args) const
{
    if (method) {
        for (const auto observer : _observers) {
            ((*observer).*method)(GetId(), std::forward<Args>(args)...);
        }
    }
}

void ProducerTranslator::onProducerStreamRegistered(const std::shared_ptr<StreamInfo>& streamInfo,
                                                    uint32_t mappedSsrc, bool registered)
{
    if (streamInfo) {
        if (const auto mime = streamInfo->GetMime()) {
            InvokeObserverMethod(&ProducerObserver::onProducerStreamRegistered,
                                 mime->IsAudioCodec(), streamInfo->GetSsrc(),
                                 mappedSsrc, registered);
        }
    }
}

void ProducerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ProducerObserver::OnProducerPauseChanged, pause);
}

ProducerTranslator::StreamInfo::StreamInfo(uint32_t sampleRate, uint32_t mappedSsrc, uint32_t ssrc)
    : _sampleRate(sampleRate)
    , _mappedSsrc(mappedSsrc)
    , _ssrc(ssrc)
{
}

ProducerTranslator::StreamInfo::~StreamInfo()
{
    SetOutputDevice(nullptr);
}

MimeChangeStatus ProducerTranslator::StreamInfo::SetMime(const RtpCodecMimeType& mime)
{
    MimeChangeStatus status = MimeChangeStatus::Failed;
    LOCK_WRITE_PROTECTED_OBJ(_depacketizer);
    if (_depacketizer->get() && mime == _depacketizer->get()->GetCodecMimeType()) {
        status = MimeChangeStatus::NotChanged;
    }
    else {
        if (auto serializer = RtpMediaFrameSerializer::create(mime)) {
            if (auto depacketizer = RtpDepacketizer::create(mime, _sampleRate)) {
                LOCK_WRITE_PROTECTED_OBJ(_serializer);
                _serializer = std::move(serializer);
                _depacketizer = std::move(depacketizer);
                status = MimeChangeStatus::Changed;
            }
        }
    }
    if (MimeChangeStatus::Changed == status) {
#ifdef WRITE_PRODUCER_RECV_TO_FILE
        if (EnsureFileWriter(mime)) {
            SetLiveMode(false);
        }
#endif
        LOCK_READ_PROTECTED_OBJ(_outputDevices);
        if (!_outputDevices->empty()) {
            SetOutputDevice(this);
        }
    }
    return status;
}

std::optional<RtpCodecMimeType> ProducerTranslator::StreamInfo::GetMime() const
{
    LOCK_READ_PROTECTED_OBJ(_depacketizer);
    if (const auto& depacketizer = _depacketizer.ConstRef()) {
        return depacketizer->GetCodecMimeType();
    }
    return std::nullopt;
}

void ProducerTranslator::StreamInfo::DepacketizeAndSerialize(const RtpPacket* packet) const
{
    if (packet) {
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

bool ProducerTranslator::StreamInfo::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        LOCK_WRITE_PROTECTED_OBJ(_outputDevices);
        if (!_outputDevices->count(outputDevice)) {
            _outputDevices->insert(outputDevice);
            if (1UL == _outputDevices->size()) {
                SetLiveMode(true);
                SetOutputDevice(this);
            }
        }
        return true;
    }
    return false;
}

bool ProducerTranslator::StreamInfo::RemoveOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        LOCK_WRITE_PROTECTED_OBJ(_outputDevices);
        const auto it = _outputDevices->find(outputDevice);
        if (it != _outputDevices->end()) {
            _outputDevices->erase(it);
            if (_outputDevices->empty()) {
                SetOutputDevice(nullptr);
            }
            return true;
        }
    }
    return false;
}

#ifdef WRITE_PRODUCER_RECV_TO_FILE
std::string_view ProducerTranslator::StreamInfo::GetFileExtension(const RtpCodecMimeType& mime) const
{
    LOCK_READ_PROTECTED_OBJ(_serializer);
    if (const auto& serializer = _serializer.ConstRef()) {
        return serializer->GetFileExtension(mime);
    }
    return {};
}

bool ProducerTranslator::StreamInfo::EnsureFileWriter(const RTC::RtpCodecMimeType& mime)
{
    LOCK_WRITE_PROTECTED_OBJ(_fileWriter);
    if (auto fileWriter = _fileWriter.Take()) {
        RemoveOutputDevice(fileWriter.get());
    }
    const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
    if (depacketizerPath && std::strlen(depacketizerPath)) {
        const auto& type = MimeTypeToString(mime);
        if (!type.empty()) {
            const auto extension = GetFileExtension(mime);
            if (!extension.empty()) {
                std::string fileName = type + std::to_string(GetMappedSsrc()) + "." + std::string(extension);
                fileName = std::string(depacketizerPath) + "/" + fileName;
                auto fileWriter = std::make_unique<FileWriter>(fileName);
                if (fileWriter->IsOpen()) {
                    if (AddOutputDevice(fileWriter.get())) {
                        _fileWriter = std::move(fileWriter);
                    }
                    else {
                        fileWriter->Close();
                        ::remove(fileName.c_str()); // from stdio.h
                    }
                }
            }
        }
    }
    return nullptr != _fileWriter->get();
}
#endif

void ProducerTranslator::StreamInfo::SetOutputDevice(OutputDevice* outputDevice) const
{
    LOCK_READ_PROTECTED_OBJ(_serializer);
    if (const auto& serializer = _serializer.ConstRef()) {
        serializer->SetOutputDevice(outputDevice);
    }
}

void ProducerTranslator::StreamInfo::SetLiveMode(bool live) const
{
    LOCK_READ_PROTECTED_OBJ(_serializer);
    if (const auto& serializer = _serializer.ConstRef()) {
        serializer->SetLiveMode(live);
    }
}

void ProducerTranslator::StreamInfo::BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
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

void ProducerTranslator::StreamInfo::EndWriteMediaPayload(uint32_t ssrc, bool ok)
{
    LOCK_READ_PROTECTED_OBJ(_outputDevices);
    for (const auto outputDevice : _outputDevices.ConstRef()) {
        outputDevice->EndWriteMediaPayload(ssrc, ok);
    }
}

void ProducerTranslator::StreamInfo::Write(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (buffer) {
        LOCK_READ_PROTECTED_OBJ(_outputDevices);
        for (const auto outputDevice : _outputDevices.ConstRef()) {
            outputDevice->Write(buffer);
        }
    }
}

} // namespace RTC
