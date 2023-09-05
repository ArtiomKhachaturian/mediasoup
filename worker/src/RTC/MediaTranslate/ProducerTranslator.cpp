#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/MediaPacketsSink.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
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

class ProducerTranslator::StreamInfo
{
public:
    StreamInfo(uint32_t sampleRate, uint32_t ssrc);
    uint32_t GetSsrc() const { return _ssrc; }
    void SetSsrc(uint32_t ssrc) { _ssrc = ssrc; }
    MimeChangeStatus SetDepacketizer(const RtpCodecMimeType& mime);
    const RtpCodecMimeType& GetMime() const;
    std::shared_ptr<RtpMediaFrame> Depacketize(const RtpPacket* packet) const;
private:
    static inline const RtpCodecMimeType _invalidMime;
    const uint32_t _sampleRate;
    uint32_t _ssrc;
    std::unique_ptr<RtpDepacketizer> _depacketizer;
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
    SetSink(nullptr);
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

bool ProducerTranslator::SetSink(const std::shared_ptr<MediaPacketsSink>& sink)
{
    if (sink != _sink) {
        bool ok = false;
        if (sink && !_streams.empty()) {
            std::list<RtpCodecMimeType> mimes;
            for (auto it = _streams.begin(); it != _streams.end(); ++it) {
                if (sink->RegistertSerializer(it->second->GetMime())) {
                    mimes.push_back(it->second->GetMime());
                }
                else {
                    const auto desc = GetStreamInfoString(it->first, it->second->GetSsrc(),
                                                          it->second->GetMime());
                    MS_ERROR("unable to find serializer for stream %s", desc.c_str());
                    break;
                }
            }
            ok = _streams.size() == mimes.size();
            if (!ok) { // rollback mimes registration
                for (const auto& mime : mimes) {
                    sink->UnRegisterSerializer(mime);
                }
            }
        }
        else {
            ok = true;
        }
        if (ok) {
            if (_sink) {
                for (auto it = _streams.begin(); it != _streams.end(); ++it) {
                    _sink->UnRegisterSerializer(it->second->GetMime());
#ifdef WRITE_PRODUCER_RECV_TO_FILE
                    DestroyFileWriter(it->second->GetMime(), it->first);
#endif
                }
            }
            _sink = sink;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
            if (ok) {
                for (auto it = _streams.begin(); it != _streams.end(); ++it) {
                    EnsureFileWriter(it->second->GetMime(), it->first);
                }
            }
#endif
        }
        return ok;
    }
    return true;
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
                const auto streamInfo = std::make_shared<StreamInfo>(stream->GetClockRate(), stream->GetSsrc());
                ok = MimeChangeStatus::Changed == streamInfo->SetDepacketizer(mime);
                if (ok) {
                    ok = !_sink || _sink->RegistertSerializer(mime);
                    if (ok) {
                        _streams[mappedSsrc] = streamInfo;
                        onProducerStreamRegistered(streamInfo, mappedSsrc, true);
#ifdef WRITE_PRODUCER_RECV_TO_FILE
                        EnsureFileWriter(mime, mappedSsrc);
#endif
                    }
                    else {
                        const auto desc = GetStreamInfoString(mappedSsrc, stream);
                        MS_ERROR("unable to find serializer for stream %s", desc.c_str());
                    }
                }
                else {
                    const auto desc = GetStreamInfoString(mappedSsrc, stream);
                    MS_ERROR("no depacketizer found for stream %s", desc.c_str());
                }
            }
            else {
                const auto& streamInfo = it->second;
                const auto ssrcChanged = streamInfo->GetSsrc() != stream->GetSsrc();
                if (ssrcChanged) {
                    onProducerStreamRegistered(streamInfo, mappedSsrc, false);
                    streamInfo->SetSsrc(stream->GetSsrc());
                }
                const auto& oldMime = streamInfo->GetMime();
                if (oldMime != mime) {
                    switch (streamInfo->SetDepacketizer(mime)) {
                        case MimeChangeStatus::Changed:
                            if (_sink) {
                                _sink->UnRegisterSerializer(mime);
                                ok = _sink->RegistertSerializer(mime);
                            }
                            else {
                                ok = true;
                            }
                            break;
                        case MimeChangeStatus::NotChanged:
                            ok = true;
                            break;
                        case MimeChangeStatus::Failed:
                            {
                                const auto desc = GetStreamInfoString(mappedSsrc, stream);
                                MS_ERROR("no depacketizer found for stream %s", desc.c_str());
                            }
                            break;
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
            if (_sink) {
                _sink->UnRegisterSerializer(streamInfo->GetMime());
            }
#ifdef WRITE_PRODUCER_RECV_TO_FILE
            DestroyFileWriter(streamInfo->GetMime(), mappedSsrc);
#endif
            _streams.erase(it);
            onProducerStreamRegistered(streamInfo, mappedSsrc, false);
            return true;
        }
    }
    return false;
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
        if (_sink) {
            const auto it = _streams.find(packet->GetSsrc());
            if (it != _streams.end()) {
                _sink->Push(it->second->Depacketize(packet));
            }
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

#ifdef WRITE_PRODUCER_RECV_TO_FILE
void ProducerTranslator::EnsureFileWriter(const RTC::RtpCodecMimeType& mime, uint32_t mappedSsrc)
{
    if (_sink && mime.IsMediaCodec()) {
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            const auto& type = MimeTypeToString(mime);
            if (!type.empty()) {
                auto fileExtension = _sink->GetFileExtension(mime);
                if (fileExtension.empty()) {
                    fileExtension = MimeSubTypeToString(mime);
                }
                std::string fileName = type + std::to_string(mappedSsrc) + "." + std::string(fileExtension);
                fileName = std::string(depacketizerPath) + "/" + fileName;
                auto fileWriter = std::make_unique<FileWriter>(fileName);
                if (fileWriter->IsOpen()) {
                    if (_sink->AddOutputDevice(fileWriter.get())) {
                        _mediaFileWriters[mappedSsrc] = std::move(fileWriter);
                    }
                    else {
                        fileWriter->Close();
                        ::remove(fileName.c_str()); // from stdio.h
                    }
                }
            }
        }
    }
}

void ProducerTranslator::DestroyFileWriter(const RTC::RtpCodecMimeType& mime, uint32_t mappedSsrc)
{
    if (mime.IsMediaCodec()) {
        const auto it = _mediaFileWriters.find(mappedSsrc);
        if (it != _mediaFileWriters.end()) {
            if (_sink) {
                _sink->UnRegisterSerializer(mime);
            }
            _mediaFileWriters.erase(it);
        }
    }
}

#endif

ProducerTranslator::StreamInfo::StreamInfo(uint32_t sampleRate, uint32_t ssrc)
    : _sampleRate(sampleRate)
    , _ssrc(ssrc)
{
}

MimeChangeStatus ProducerTranslator::StreamInfo::SetDepacketizer(const RtpCodecMimeType& mime)
{
    MimeChangeStatus status = MimeChangeStatus::Failed;
    if (mime) {
        if (_depacketizer && mime == _depacketizer->GetCodecMimeType()) {
            status = MimeChangeStatus::NotChanged;
        }
        else if (auto depacketizer = RtpDepacketizer::create(mime, _sampleRate)) {
            _depacketizer = std::move(depacketizer);
            status = MimeChangeStatus::Changed;
        }
    }
    return status;
}

const RtpCodecMimeType& ProducerTranslator::StreamInfo::GetMime() const
{
    if (_depacketizer) {
        return _depacketizer->GetCodecMimeType();
    }
    return _invalidMime;
}

std::shared_ptr<RtpMediaFrame> ProducerTranslator::StreamInfo::
    Depacketize(const RtpPacket* packet) const
{
    if (_depacketizer) {
        return _depacketizer->AddPacket(packet);
    }
    return nullptr;
}

} // namespace RTC
