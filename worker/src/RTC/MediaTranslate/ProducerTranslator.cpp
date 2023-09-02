#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/MediaPacketsSink.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#ifdef WRITE_PRODUCER_RECV_AUDIO_TO_FILE
#include "RTC/MediaTranslate/MediaFileWriter.hpp"
#endif
#include "RTC/RtpStream.hpp"
#include "Logger.hpp"

namespace {

inline bool IsAudioStream(const RTC::RtpStream* stream) {
    return stream && RTC::RtpCodecMimeType::Type::AUDIO == stream->GetMimeType().type;
}

inline bool IsVideoStream(const RTC::RtpStream* stream) {
    return stream && RTC::RtpCodecMimeType::Type::VIDEO == stream->GetMimeType().type;
}

}

namespace RTC
{

class ProducerTranslator::MediaPacketsSinkImpl
{
    // 1st - original SSRC, 2nd - mapped
    using SsrcMapping = std::pair<uint32_t, uint32_t>;
public:
    MediaPacketsSinkImpl();
    bool SetAudioCodecType(RtpCodecMimeType::Subtype codecType);
    const RtpCodecMimeType& GetAudioCodec() const { return _audioMime; }
    bool SetVideoCodecType(RtpCodecMimeType::Subtype codecType);
    void ResetVideoCodecType() { SetVideoCodecType(RtpCodecMimeType::Subtype::UNSET); }
    const RtpCodecMimeType& GetVideoCodec() const { return _videoMime; }
    bool AddOutputDevice(OutputDevice* outputDevice);
    bool RemoveOutputDevice(OutputDevice* outputDevice);
    void AddPacket(bool audio, const RtpPacket* packet);
private:
    static void AddPacket(const RtpPacket* packet,
                          const RtpCodecMimeType& mime,
                          const std::shared_ptr<MediaPacketsSink>& sink);
private:
    RtpCodecMimeType _audioMime;
    RtpCodecMimeType _videoMime;
    std::shared_ptr<MediaPacketsSink> _audioSink;
    std::shared_ptr<MediaPacketsSink> _videoSink;
};

ProducerTranslator::ProducerTranslator(const std::string& id,
                                       const std::weak_ptr<ProducerObserver>& observerRef)
    : TranslatorUnitImpl<ProducerObserver, ProducerInputMediaStreamer>(id, observerRef)
{
}

ProducerTranslator::~ProducerTranslator()
{
    for (const auto ssrc : GetRegisteredAudio()) {
        UnRegisterAudio(ssrc);
    }
    for (const auto ssrc : GetRegisteredVideo()) {
        UnRegisterVideo(ssrc);
    }
}

bool ProducerTranslator::AddOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto ita = _sinks.find(audioSsrc);
        if (ita != _sinks.end()) {
            return ita->second->AddOutputDevice(outputDevice);
        }
    }
    return false;
}

bool ProducerTranslator::RemoveOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto ita = _sinks.find(audioSsrc);
        if (ita != _sinks.end()) {
            return ita->second->RemoveOutputDevice(outputDevice);
        }
    }
    return false;
}

std::list<uint32_t> ProducerTranslator::GetRegisteredAudio() const
{
    std::list<uint32_t> ssrcs;
    for (auto it = _sinks.begin(); it != _sinks.end(); ++it) {
        ssrcs.push_back(it->first);
    }
    return ssrcs;
}

bool ProducerTranslator::IsAudioRegistered(uint32_t ssrc) const
{
    return ssrc && _sinks.count(ssrc);
}

bool ProducerTranslator::RegisterAudio(uint32_t ssrc, uint32_t mappedSsrc,
                                       RtpCodecMimeType::Subtype codecType)
{
    if (ssrc && mappedSsrc) {
        if (_sinks.end() == _sinks.find(ssrc)) {
            auto sink = std::make_unique<MediaPacketsSinkImpl>();
            if (sink->SetAudioCodecType(codecType)) {
                _audioSsrcToMappedSsrc[ssrc] = mappedSsrc;
                _audioMappedSsrcToSsrc[mappedSsrc] = ssrc;
                const auto itDubbing = _audioSsrcToVideoSsrc.find(ssrc);
                if (itDubbing != _audioSsrcToVideoSsrc.end()) {
                    const auto itMime = _videoSsrcToCodecType.find(itDubbing->second);
                    if (itMime != _videoSsrcToCodecType.end()) {
                        if (!sink->SetVideoCodecType(itMime->second)) {
                            // TODO: log error
                        }
                    }
                }
#ifdef WRITE_PRODUCER_RECV_AUDIO_TO_FILE
                const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
                if (depacketizerPath && std::strlen(depacketizerPath)) {
                    const auto liveMode = false;
                    auto fileWriter = MediaFileWriter::Create(depacketizerPath,
                                                              sink->GetAudioCodec(),
                                                              mappedSsrc,
                                                              liveMode);
                    if (fileWriter) {
                        _audioFileWriters[mappedSsrc] = std::move(fileWriter);
                    }
                }
#endif
                _sinks[ssrc] = std::move(sink);
                if (const auto observer = _observerRef.lock()) {
                    observer->onProducerMediaRegistered(GetId(), true, ssrc, mappedSsrc, true);
                }
                return true;
            }
        }
    }
    return false;
}

bool ProducerTranslator::RegisterAudio(const RtpStream* audioStream, uint32_t mappedSsrc)
{
    if (mappedSsrc && IsAudioStream(audioStream)) {
        return RegisterAudio(audioStream->GetSsrc(), mappedSsrc, audioStream->GetMimeType().subtype);
    }
    return false;
}

bool ProducerTranslator::UnRegisterAudio(uint32_t ssrc)
{
    if (ssrc) {
        const auto its = _sinks.find(ssrc);
        if (its != _sinks.end()) {
            uint32_t mappedSsrc = 0U;
            // remove mappings
            const auto itMapped = _audioSsrcToMappedSsrc.find(ssrc);
            if (itMapped != _audioSsrcToMappedSsrc.end()) {
                mappedSsrc = itMapped->second;
#ifdef WRITE_PRODUCER_RECV_AUDIO_TO_FILE
                _audioFileWriters.erase(mappedSsrc);
#endif
                _audioMappedSsrcToSsrc.erase(mappedSsrc);
                _audioSsrcToMappedSsrc.erase(itMapped);
            }
            // break dubbing
            RemoveVideoDubbing(ssrc, true);
            _sinks.erase(its);
            if (const auto observer = _observerRef.lock()) {
                observer->onProducerMediaRegistered(GetId(), true, ssrc, mappedSsrc, false);
            }
            return true;
        }
    }
    return false;
}

bool ProducerTranslator::UnRegisterAudio(const RtpStream* audioStream)
{
    return IsAudioStream(audioStream) && UnRegisterAudio(audioStream->GetSsrc());
}

std::list<uint32_t> ProducerTranslator::GetRegisteredVideo() const
{
    std::list<uint32_t> ssrcs;
    for (auto it = _videoSsrcToCodecType.begin(); it != _videoSsrcToCodecType.end(); ++it) {
        ssrcs.push_back(it->first);
    }
    return ssrcs;
}

bool ProducerTranslator::IsVideoRegistered(uint32_t ssrc) const
{
    return ssrc && _videoSsrcToCodecType.count(ssrc);
}

bool ProducerTranslator::RegisterVideo(uint32_t ssrc, uint32_t mappedSsrc,
                                       RtpCodecMimeType::Subtype codecType)
{
    if (ssrc && mappedSsrc && RtpCodecMimeType::Subtype::UNSET != codecType) {
        if (_videoSsrcToCodecType.end() == _videoSsrcToCodecType.find(ssrc)) {
            _videoSsrcToCodecType[ssrc] = codecType;
            _videoSsrcToMappedSsrc[ssrc] = mappedSsrc;
            _videoMappedSsrcToSsrc[mappedSsrc] = ssrc;
            if (const auto observer = _observerRef.lock()) {
                observer->onProducerMediaRegistered(GetId(), false, ssrc, mappedSsrc, true);
            }
            return true;
        }
    }
    return false;
}

bool ProducerTranslator::RegisterVideo(const RtpStream* videoStream, uint32_t mappedSsrc)
{
    if (mappedSsrc && IsVideoStream(videoStream)) {
        return RegisterVideo(videoStream->GetSsrc(), mappedSsrc,
                             videoStream->GetMimeType().subtype);
    }
    return false;
}

bool ProducerTranslator::UnRegisterVideo(uint32_t ssrc)
{
    if (ssrc && _videoSsrcToCodecType.erase(ssrc)) {
        uint32_t mappedSsrc = 0U;
        // remove mappings
        const auto itMapped = _videoSsrcToMappedSsrc.find(ssrc);
        if (itMapped != _videoSsrcToMappedSsrc.end()) {
            mappedSsrc = itMapped->second;
            _videoMappedSsrcToSsrc.erase(mappedSsrc);
            _videoSsrcToMappedSsrc.erase(itMapped);
        }
        // break dubbing
        RemoveVideoDubbing(ssrc, false);
        if (const auto observer = _observerRef.lock()) {
            observer->onProducerMediaRegistered(GetId(), false, ssrc, mappedSsrc, false);
        }
        return true;
    }
    return false;
}

bool ProducerTranslator::UnRegisterVideo(const RtpStream* videoStream)
{
    return IsVideoStream(videoStream) && UnRegisterVideo(videoStream->GetSsrc());
}

bool ProducerTranslator::RegisterVideoDubbing(uint32_t audioSsrc, uint32_t videoSsrc)
{
    if (audioSsrc && videoSsrc &&
        !_audioSsrcToVideoSsrc.count(audioSsrc) &&
        !_videoSsrcToAudioSsrc.count(videoSsrc)) {
        const auto it = _sinks.find(audioSsrc);
        if (it != _sinks.end()) {
            const auto itMime = _videoSsrcToCodecType.find(videoSsrc);
            if (itMime != _videoSsrcToCodecType.end() &&
                it->second->SetVideoCodecType(itMime->second)) {
                _videoSsrcToAudioSsrc[videoSsrc] = audioSsrc;
                _audioSsrcToVideoSsrc[audioSsrc] = videoSsrc;
                return true;
            }
        }
    }
    return false;
}

bool ProducerTranslator::UnRegisterVideoDubbing(uint32_t audioSsrc, uint32_t videoSsrc)
{
    if (_audioSsrcToVideoSsrc.erase(audioSsrc) && _videoSsrcToAudioSsrc.erase(videoSsrc) ) {
        const auto it = _sinks.find(audioSsrc);
        if (it != _sinks.end()) {
            it->second->SetVideoCodecType(RtpCodecMimeType::Subtype::UNSET);
        }
        return true;
    }
    return false;
}

void ProducerTranslator::AddPacket(const RtpPacket* packet)
{
    if (packet && !IsPaused()) {
        bool isAudioSsrc = false;
        if (const auto sink = GetSink(packet->GetSsrc(), isAudioSsrc)) {
            sink->AddPacket(isAudioSsrc, packet);
#ifdef WRITE_PRODUCER_RECV_AUDIO_TO_FILE
            if (isAudioSsrc) {
                const auto ita = _audioFileWriters.find(packet->GetSsrc());
                if (ita != _audioFileWriters.end()) {
                    ita->second->AddPacket(packet);
                }
            }
#endif
        }
    }
}

void ProducerTranslator::SetLanguage(const std::optional<MediaLanguage>& language)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_language);
        if (_language.ConstRef() != language) {
            _language = language;
            changed = true;
        }
    }
    if (changed) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnProducerLanguageChanged(GetId());
        }
    }
}

std::optional<MediaLanguage> ProducerTranslator::GetLanguage() const
{
    LOCK_READ_PROTECTED_OBJ(_language);
    return _language;
}

void ProducerTranslator::OnPauseChanged(bool pause)
{
    if (const auto observer = _observerRef.lock()) {
        observer->OnProducerPauseChanged(GetId(), pause);
    }
}

ProducerTranslator::MediaPacketsSinkImpl* ProducerTranslator::GetSink(uint32_t mappedSsrc,
                                                                      bool& isAudioSsrc) const
{
    if (mappedSsrc && !_sinks.empty()) {
        uint32_t ssrc = GetAudioSsrc(mappedSsrc);
        if (0U == ssrc) {
            ssrc = GetVideoSsrc(mappedSsrc);
            if (ssrc) {
                // find original audio SSRC by  original video SSRC
                const auto it = _videoSsrcToAudioSsrc.find(ssrc);
                if (it != _videoSsrcToAudioSsrc.end()) {
                    ssrc = it->second;
                }
                else {
                    ssrc = 0U;
                }
            }
        }
        else {
            isAudioSsrc = true;
        }
        if (ssrc) {
            const auto it = _sinks.find(ssrc);
            if (it != _sinks.end()) {
                return it->second.get();
            }
        }
    }
    return nullptr;
}

uint32_t ProducerTranslator::GetAudioSsrc(uint32_t mappedSsrc) const
{
    if (mappedSsrc) {
        const auto it = _audioMappedSsrcToSsrc.find(mappedSsrc);
        if (it != _audioMappedSsrcToSsrc.end()) {
            return it->second;
        }
    }
    return 0U;
}

uint32_t ProducerTranslator::GetVideoSsrc(uint32_t mappedSsrc) const
{
    if (mappedSsrc) {
        const auto it = _videoMappedSsrcToSsrc.find(mappedSsrc);
        if (it != _videoMappedSsrcToSsrc.end()) {
            return it->second;
        }
    }
    return 0U;
}

void ProducerTranslator::RemoveVideoDubbing(uint32_t ssrc, bool isAudioSsrc)
{
    if (ssrc) {
        auto& original = isAudioSsrc ? _audioSsrcToVideoSsrc : _videoSsrcToAudioSsrc;
        const auto it = original.find(ssrc);
        if (it != original.end()) {
            auto& mapped = isAudioSsrc ? _videoSsrcToAudioSsrc : _audioSsrcToVideoSsrc;
            mapped.erase(it->second);
            original.erase(it);
        }
    }
}

ProducerTranslator::MediaPacketsSinkImpl::MediaPacketsSinkImpl()
{
    _audioMime.type = RtpCodecMimeType::Type::AUDIO;
    _videoMime.type = RtpCodecMimeType::Type::AUDIO;
    _audioMime.subtype = _videoMime.subtype = RtpCodecMimeType::Subtype::UNSET;
}

bool ProducerTranslator::MediaPacketsSinkImpl::SetAudioCodecType(RtpCodecMimeType::Subtype codecType)
{
    bool ok = false;
    if (codecType != _audioMime.subtype) {
        if (RtpCodecMimeType::Subtype::UNSET == codecType) {
            _audioSink.reset();
            ok = true;
        }
        else {
            RtpCodecMimeType audioMime;
            audioMime.type = _audioMime.type;
            audioMime.subtype = codecType;
            ok = _audioSink && _audioSink->IsCompatible(audioMime);
            if (!ok) {
                ok = _videoSink && _videoSink->IsCompatible(audioMime);
                if (ok) {
                    _audioSink = _videoSink;
                }
                else {
                    if (auto serializer = RtpMediaFrameSerializer::create(audioMime)) {
                        _audioSink = std::make_shared<MediaPacketsSink>(std::move(serializer));
                        ok = true;
                    }
                }
            }
            if (ok) {
                _audioMime.subtype = codecType;
            }
        }
    }
    else {
        ok = true;
    }
    return ok;
}

bool ProducerTranslator::MediaPacketsSinkImpl::SetVideoCodecType(RtpCodecMimeType::Subtype codecType)
{
    bool ok = false;
    if (codecType != _videoMime.subtype) {
        if (RtpCodecMimeType::Subtype::UNSET == codecType) {
            _videoSink.reset();
            ok = true;
        }
        else {
            RtpCodecMimeType videoMime;
            videoMime.type = _videoMime.type;
            videoMime.subtype = codecType;
            ok = _videoSink && _videoSink->IsCompatible(videoMime);
            if (!ok) {
                ok = _audioSink && _audioSink->IsCompatible(videoMime);
                if (ok) {
                    _videoSink = _audioSink;
                }
                else {
                    if (auto serializer = RtpMediaFrameSerializer::create(videoMime)) {
                        _videoSink = std::make_shared<MediaPacketsSink>(std::move(serializer));
                        ok = true;
                    }
                }
            }
            if (ok) {
                _videoMime.subtype = codecType;
            }
        }
    }
    else {
        ok = true;
    }
    return ok;
}

bool ProducerTranslator::MediaPacketsSinkImpl::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto b1 = _audioSink && _audioSink->AddOutputDevice(outputDevice);
        const auto b2 = _videoSink && _videoSink->AddOutputDevice(outputDevice);
        return b1 || b2;
    }
    return false;
}

bool ProducerTranslator::MediaPacketsSinkImpl::RemoveOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto b1 = _audioSink && _audioSink->RemoveOutputDevice(outputDevice);
        const auto b2 = _videoSink && _videoSink->RemoveOutputDevice(outputDevice);
        return b1 || b2;
    }
    return false;
}

void ProducerTranslator::MediaPacketsSinkImpl::AddPacket(bool audio, const RtpPacket* packet)
{
    if (packet) {
        AddPacket(packet, audio ? _audioMime : _videoMime, audio ? _audioSink : _videoSink);
    }
}

void ProducerTranslator::MediaPacketsSinkImpl::AddPacket(const RtpPacket* packet,
                                                         const RtpCodecMimeType& mime,
                                                         const std::shared_ptr<MediaPacketsSink>& sink)
{
    if (packet && sink) {
        sink->AddPacket(mime, packet);
    }
    else {
        // TODO: log error
    }
}

} // namespace RTC
