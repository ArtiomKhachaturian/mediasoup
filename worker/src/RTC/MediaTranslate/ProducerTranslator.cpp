#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/MediaPacketsSink.hpp"
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

class ProducerTranslator::StreamInfo
{
public:
    StreamInfo(const std::shared_ptr<MediaPacketsSink>& sink,
               uint32_t sampleRate, uint32_t mappedSsrc, uint32_t ssrc);
    ~StreamInfo();
    uint32_t GetMappedSsrc() const { return _mappedSsrc; }
    void SetSsrc(uint32_t ssrc) { _ssrc = ssrc; }
    MimeChangeStatus SetMime(const RtpCodecMimeType& mime);
    const RtpCodecMimeType& GetMime() const;
    void DepacketizeAndSerialize(const RtpPacket* packet) const;
    uint32_t GetSsrc() const { return _ssrc; }
private:
    static inline const RtpCodecMimeType _invalidMime;
    const std::shared_ptr<MediaPacketsSink> _sink;
    const uint32_t _sampleRate;
    const uint32_t _mappedSsrc;
    std::atomic<uint32_t> _ssrc;
    ProtectedUniquePtr<RtpDepacketizer> _depacketizer;
};

ProducerTranslator::ProducerTranslator(Producer* producer)
    : _producer(producer)
    , _sink(std::make_shared<MediaPacketsSink>())
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
    return mappedSsrc && stream && RegisterStream(stream->GetMimeType(),
                                                  stream->GetClockRate(),
                                                  mappedSsrc, stream->GetSsrc());
}

bool ProducerTranslator::RegisterStream(const RtpCodecMimeType& mime, uint32_t clockRate,
                                        uint32_t mappedSsrc, uint32_t ssrc)
{
    bool ok = false;
    if (mappedSsrc && mime.IsMediaCodec()) {
        MS_ASSERT(mime.IsAudioCodec() == IsAudio(), "mime types mistmatch");
        MS_ASSERT(clockRate, "clock rate must be greater than zero");
        const auto it = _streams.find(mappedSsrc);
        if (it == _streams.end()) {
            const auto streamInfo = std::make_shared<StreamInfo>(_sink, clockRate, mappedSsrc, ssrc);
            ok = MimeChangeStatus::Changed == streamInfo->SetMime(mime);
            if (ok) {
                bool mediaRegistered = false;
                if (IsAudio()) {
                    mediaRegistered = _sink->RegisterAudio(mappedSsrc, mime.GetSubtype());
                }
                else {
                    mediaRegistered = _sink->RegisterVideo(mappedSsrc, mime.GetSubtype());
                }
                if (mediaRegistered) {
                    _streams[mappedSsrc] = streamInfo;
                    onProducerStreamRegistered(streamInfo, mappedSsrc, true);
#ifdef WRITE_PRODUCER_RECV_TO_FILE // 1st stream is registered
                    if (1UL == _streams.size()) {
                        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
                        if (depacketizerPath && std::strlen(depacketizerPath)) {
                            const auto extension = _sink->GetFileExtension(mime);
                            if (!extension.empty()) {
                                const std::string type(IsAudio() ? "audio" : "video");
                                std::string fileName = type + GetId() + "." + std::string(extension);
                                fileName = std::string(depacketizerPath) + "/" + fileName;
                                auto fileWriter = std::make_unique<FileWriter>(fileName);
                                if (fileWriter->IsOpen()) {
                                    if (_sink->AddOutputDevice(fileWriter.get())) {
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
#endif
                }
                else {
                    const auto desc = GetStreamInfoString(mime, mappedSsrc, ssrc);
                    MS_ERROR("failed to register media serializarion for stream [%s]", desc.c_str());
                }
            }
            else {
                const auto desc = GetStreamInfoString(mime, mappedSsrc, ssrc);
                MS_ERROR("not found depacketizer or serializer for stream [%s]", desc.c_str());
            }
        }
        else {
            const auto& streamInfo = it->second;
            const auto ssrcChanged = streamInfo->GetSsrc() != ssrc;
            if (ssrcChanged) {
                onProducerStreamRegistered(streamInfo, mappedSsrc, false);
                streamInfo->SetSsrc(ssrc);
            }
            const auto& oldMime = streamInfo->GetMime();
            if (oldMime != mime) {
                ok = MimeChangeStatus::Failed != streamInfo->SetMime(mime);
                if (!ok) {
                    const auto desc = GetStreamInfoString(mime, mappedSsrc, ssrc);
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

std::shared_ptr<ProducerInputMediaStreamer> ProducerTranslator::GetMediaStreamer() const
{
    return _sink;
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
        MS_ASSERT(streamInfo->GetMime(), "invalid mime");
        InvokeObserverMethod(&ProducerObserver::onProducerStreamRegistered,
                             streamInfo->GetMime().IsAudioCodec(), streamInfo->GetSsrc(),
                             mappedSsrc, registered);
    }
}

void ProducerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ProducerObserver::OnProducerPauseChanged, pause);
}

ProducerTranslator::StreamInfo::StreamInfo(const std::shared_ptr<MediaPacketsSink>& sink,
                                           uint32_t sampleRate, uint32_t mappedSsrc, uint32_t ssrc)
    : _sink(sink)
    , _sampleRate(sampleRate)
    , _mappedSsrc(mappedSsrc)
    , _ssrc(ssrc)
{
}

ProducerTranslator::StreamInfo::~StreamInfo()
{
    _sink->UnRegisterSerializer(GetMime());
}

MimeChangeStatus ProducerTranslator::StreamInfo::SetMime(const RtpCodecMimeType& mime)
{
    MimeChangeStatus status = MimeChangeStatus::Failed;
    if (mime) {
        LOCK_WRITE_PROTECTED_OBJ(_depacketizer);
        if (_depacketizer->get() && mime == _depacketizer->get()->GetCodecMimeType()) {
            status = MimeChangeStatus::NotChanged;
        }
        else if (_sink->RegisterSerializer(mime)) {
            if (auto depacketizer = RtpDepacketizer::create(mime, _sampleRate)) {
                _depacketizer = std::move(depacketizer);
                status = MimeChangeStatus::Changed;
            }
            else {
                _sink->UnRegisterSerializer(mime);
            }
        }
    }
    return status;
}

const RtpCodecMimeType& ProducerTranslator::StreamInfo::GetMime() const
{
    LOCK_READ_PROTECTED_OBJ(_depacketizer);
    if (const auto& depacketizer = _depacketizer.ConstRef()) {
        return depacketizer->GetCodecMimeType();
    }
    return _invalidMime;
}

void ProducerTranslator::StreamInfo::DepacketizeAndSerialize(const RtpPacket* packet) const
{
    if (packet) {
        LOCK_READ_PROTECTED_OBJ(_depacketizer);
        if (const auto& depacketizer = _depacketizer.ConstRef()) {
            _sink->Push(depacketizer->AddPacket(packet));
        }
    }
}

} // namespace RTC
