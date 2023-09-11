#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
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

class ProducerTranslator::StreamInfo
{
public:
    StreamInfo(uint32_t clockRate, uint32_t mappedSsrc);
    ~StreamInfo();
    uint32_t GetClockRate() const { return _clockRate; }
    uint32_t GetMappedSsrc() const { return _mappedSsrc; }
    MimeChangeStatus SetMime(const RtpCodecMimeType& mime);
    const RtpCodecMimeType& GetMime() const;
    std::shared_ptr<const RtpMediaFrame> Depacketize(const RtpPacket* packet) const;
private:
    static inline const RtpCodecMimeType _invalidMime;
    const uint32_t _clockRate;
    const uint32_t _mappedSsrc;
    ProtectedUniquePtr<RtpDepacketizer> _depacketizer;
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
            const RtpCodecMimeType mime(it->second->GetMime());
            _streams.erase(it);
            InvokeObserverMethod(&ProducerObserver::onStreamRemoved, mappedSsrc, mime);
            return true;
        }
    }
    return false;
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
            if (const auto frame = it->second->Depacketize(packet)) {
                InvokeObserverMethod(&ProducerObserver::OnMediaFrameProduced,
                                     packet->GetSsrc(), frame);
            }
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

template <class Method, typename... Args>
void ProducerTranslator::InvokeObserverMethod(const Method& method, Args&&... args) const
{
    _observers.InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

void ProducerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ProducerObserver::OnPauseChanged, pause);
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
    if (mime) {
        LOCK_WRITE_PROTECTED_OBJ(_depacketizer);
        if (_depacketizer->get() && mime == _depacketizer->get()->GetMimeType()) {
            status = MimeChangeStatus::NotChanged;
        }
        else {
            if (auto depacketizer = RtpDepacketizer::create(mime, GetClockRate())) {
                _depacketizer = std::move(depacketizer);
                status = MimeChangeStatus::Changed;
            }
        }
    }
    return status;
}

const RtpCodecMimeType& ProducerTranslator::StreamInfo::GetMime() const
{
    LOCK_READ_PROTECTED_OBJ(_depacketizer);
    if (const auto& depacketizer = _depacketizer.ConstRef()) {
        return depacketizer->GetMimeType();
    }
    return _invalidMime;
}

std::shared_ptr<const RtpMediaFrame> ProducerTranslator::StreamInfo::
    Depacketize(const RtpPacket* packet) const
{
    if (packet) {
        MS_ASSERT(packet->GetSsrc() == GetMappedSsrc(), "invalid SSRC mapping");
        LOCK_READ_PROTECTED_OBJ(_depacketizer);
        if (const auto& depacketizer = _depacketizer.ConstRef()) {
            return depacketizer->AddPacket(packet);
        }
    }
    return nullptr;
}

} // namespace RTC
