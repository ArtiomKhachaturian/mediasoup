#define MS_CLASS "RTC::Translator"
#include "RTC/MediaTranslate/Translator.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/TranslatorSource.hpp"
#if defined(NO_TRANSLATION_SERVICE) || defined(SINGLE_TRANSLATION_POINT_CONNECTION)
#include "RTC/MediaTranslate/TranslatorEndPoint/StubEndPoint.hpp"
#endif
#ifdef NO_TRANSLATION_SERVICE
#include "RTC/MediaTranslate/TranslatorEndPoint/FileEndPoint.hpp"
#else
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketEndPoint.hpp"
#endif
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"

namespace RTC
{

Translator::Translator(const Producer* producer, RtpPacketsPlayer* rtpPacketsPlayer,
                       RtpPacketsCollector* output)
    : _producer(producer)
    , _rtpPacketsPlayer(rtpPacketsPlayer)
    , _output(output)
{
}

Translator::~Translator()
{
    {
        LOCK_WRITE_PROTECTED_OBJ(_consumers);
        _consumers->clear();
    }
    for (const auto mappedSsrc : GetSsrcs(true)) {
        RemoveStream(mappedSsrc);
    }
}

std::unique_ptr<Translator> Translator::Create(const Producer* producer,
                                               RtpPacketsPlayer* rtpPacketsPlayer,
                                               RtpPacketsCollector* output)
{
    if (producer && rtpPacketsPlayer && output && Media::Kind::AUDIO == producer->GetKind()) {
        auto translator = new Translator(producer, rtpPacketsPlayer, output);
        // add streams
        const auto& streams = producer->GetRtpStreams();
        for (auto it = streams.begin(); it != streams.end(); ++it) {
            translator->AddStream(it->second, it->first);
        }
        return std::unique_ptr<Translator>(translator);
    }
    return nullptr;
}

bool Translator::AddStream(uint32_t mappedSsrc, const RtpStream* stream)
{
    bool ok = false;
    if (stream && mappedSsrc) {
        const auto& mime = stream->GetMimeType();
        if (mime.IsAudioCodec()) {
            const auto clockRate = stream->GetClockRate();
            const auto originalSsrc = stream->GetSsrc();
            const auto payloadType = stream->GetPayloadType();
            MS_ASSERT(clockRate, "clock rate must be greater than zero");
            MS_ASSERT(originalSsrc, "original SSRC must be greater than zero");
            MS_ASSERT(payloadType, "payload type must be greater than zero");
            LOCK_WRITE_PROTECTED_OBJ(_mappedSsrcToStreams);
            const auto it = _mappedSsrcToStreams->find(mappedSsrc);
            if (it == _mappedSsrcToStreams->end()) {
                auto source = TranslatorSource::Create(mime, clockRate, originalSsrc,
                                                       payloadType, this, _rtpPacketsPlayer,
                                                       _output, _producer->id);
                if (source) {
                    source->SetInputLanguage(_producer->GetLanguageId());
                    AddConsumersToSource(source);
                    _originalSsrcToStreams[originalSsrc] = source;
                    _mappedSsrcToStreams->insert({mappedSsrc, std::move(source)});
                    ok = true;
                }
                else {
                    const auto desc = GetStreamInfoString(mime, mappedSsrc);
                    MS_ERROR_STD("depacketizer or serializer is not available for stream [%s]", desc.c_str());
                }
            }
            else {
                MS_ASSERT(it->second->GetMime() == mime, "MIME type mistmatch");
                MS_ASSERT(it->second->GetClockRate() == clockRate, "clock rate mistmatch");
                MS_ASSERT(it->second->GetOriginalSsrc() == originalSsrc, "original SSRC mistmatch");
                MS_ASSERT(it->second->GetPayloadType() == payloadType, "payload type mistmatch");
                ok = true; // already registered
            }
        }
    }
    return ok;
}

bool Translator::RemoveStream(uint32_t mappedSsrc)
{
    if (mappedSsrc) {
        LOCK_WRITE_PROTECTED_OBJ(_mappedSsrcToStreams);
        const auto it = _mappedSsrcToStreams->find(mappedSsrc);
        if (it != _mappedSsrcToStreams->end()) {
            _originalSsrcToStreams.erase(it->second->GetOriginalSsrc());
            _mappedSsrcToStreams->erase(it);
            return true;
        }
    }
    return false;
}

bool Translator::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    if (packet && !_producer->IsPaused()) {
        if (const auto source = GetSource(packet->GetSsrc())) {
            const auto added = source->AddOriginalRtpPacketForTranslation(packet);
            PostProcessAfterAdding(packet, added, source);
            return added;
        }
    }
    return false;
}

const std::string& Translator::GetId() const
{
    return _producer->id;
}

std::list<uint32_t> Translator::GetSsrcs(bool mapped) const
{
    std::list<uint32_t> ssrcs;
    LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
    for (auto it = _mappedSsrcToStreams->begin(); it != _mappedSsrcToStreams->end(); ++it) {
        ssrcs.push_back(mapped ? it->first : it->second->GetOriginalSsrc());
    }
    return ssrcs;
}

void Translator::AddConsumer(Consumer* consumer)
{
    if (consumer && Media::Kind::AUDIO == consumer->GetKind()) {
        MS_ASSERT(consumer->producerId == GetId(), "wrong producer ID");
        LOCK_WRITE_PROTECTED_OBJ(_consumers);
        if (_consumers->end() == std::find(_consumers->begin(), _consumers->end(), consumer)) {
            LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
            for (auto it = _mappedSsrcToStreams->begin(); it != _mappedSsrcToStreams->end(); ++it) {
                it->second->AddConsumer(consumer);
            }
            _consumers->push_back(consumer);
        }
    }
}

void Translator::RemoveConsumer(Consumer* consumer)
{
    if (consumer && Media::Kind::AUDIO == consumer->GetKind()) {
        MS_ASSERT(consumer->producerId == GetId(), "wrong producer ID");
        LOCK_WRITE_PROTECTED_OBJ(_consumers);
        const auto it = std::find(_consumers->begin(), _consumers->end(), consumer);
        if (it != _consumers->end()) {
            LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
            for (auto it = _mappedSsrcToStreams->begin(); it != _mappedSsrcToStreams->end(); ++it) {
                it->second->RemoveConsumer(consumer);
            }
            _consumers->erase(it);
        }
    }
}

void Translator::UpdateProducerLanguage()
{
    LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
    for (auto it = _mappedSsrcToStreams->begin(); it != _mappedSsrcToStreams->end(); ++it) {
        it->second->SetInputLanguage(_producer->GetLanguageId());
    }
}

void Translator::UpdateConsumerLanguageOrVoice(Consumer* consumer)
{
    if (consumer && Media::Kind::AUDIO == consumer->GetKind()) {
        LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
        for (auto it = _mappedSsrcToStreams->begin(); it != _mappedSsrcToStreams->end(); ++it) {
            it->second->UpdateConsumer(consumer);
        }
    }
}

std::shared_ptr<TranslatorSource> Translator::GetSource(uint32_t ssrc) const
{
    if (ssrc) {
        LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
        const auto itm = _mappedSsrcToStreams->find(ssrc);
        if (itm != _mappedSsrcToStreams->end()) {
            return itm->second;
        }
        const auto ito = _originalSsrcToStreams.find(ssrc);
        if (ito != _originalSsrcToStreams.end()) {
            MS_ASSERT(!ito->second.expired(), "something went wrong with streams management");
            return ito->second.lock();
        }
    }
    return nullptr;
}

void Translator::AddConsumersToSource(const std::shared_ptr<TranslatorSource>& source) const
{
    if (source) {
        LOCK_READ_PROTECTED_OBJ(_consumers);
        for (const auto consumer : _consumers.ConstRef()) {
            source->AddConsumer(consumer);
        }
    }
}

void Translator::PostProcessAfterAdding(RtpPacket* packet, bool added,
                                        const std::shared_ptr<TranslatorSource>& source)
{
    if (packet && source) {
        LOCK_READ_PROTECTED_OBJ(_consumers);
        if (!_consumers->empty()) {
            const auto playing = _rtpPacketsPlayer->IsPlaying(source->GetOriginalSsrc());
            const auto saveInfo = !added || source->GetAddedPacketsCount() < 2UL;
            for (const auto consumer : _consumers.ConstRef()) {
                const auto reject = added && (playing || source->IsConnected(consumer));
                if (reject) {
                    packet->AddRejectedConsumer(consumer);
                }
                if (saveInfo || !reject) {
                    source->SaveProducerRtpPacketInfo(consumer, packet);
                }
            }
        }
    }
}

#ifdef NO_TRANSLATION_SERVICE
std::shared_ptr<TranslatorEndPoint> Translator::CreateStubEndPoint() const
{
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    // for the 1st producer & 1st consumer will receive audio from file as translation
    if (0U == FileEndPoint::GetInstancesCount()) {
        return CreateMaybeFileEndPoint();
    }
    return std::make_shared<StubEndPoint>(GetId());
#else
    return CreateMaybeFileEndPoint();
#endif
}

std::shared_ptr<TranslatorEndPoint> Translator::CreateMaybeFileEndPoint() const
{
    auto fileEndPoint = std::make_shared<FileEndPoint>(_mockTranslationFileName, GetId(),
                                                       _mockTranslationFileNameLenMs,
                                                       _mockTranslationConnectionTimeoutMs,
                                                       std::nullopt,
                                                       _rtpPacketsPlayer->GetTimer());
    if (!fileEndPoint->IsValid()) {
        MS_ERROR_STD("failed open %s as mock translation", _mockTranslationFileName);
    }
    else {
        _nonStubEndPointRef = fileEndPoint;
        return fileEndPoint;
    }
    return std::make_shared<StubEndPoint>(GetId());
}

#else

std::shared_ptr<TranslatorEndPoint> Translator::CreateMaybeStubEndPoint() const
{
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    if (0 == WebsocketEndPoint::GetInstancesCount()) {
        auto socketEndPoint = std::make_shared<WebsocketEndPoint>(GetId());
        _nonStubEndPointRef = socketEndPoint;
        return socketEndPoint;
    }
    return std::make_shared<StubEndPoint>(GetId());
#else
    return std::make_shared<WebsocketEndPoint>(GetId());
#endif
}
#endif

std::shared_ptr<TranslatorEndPoint> Translator::CreateEndPoint()
{
#ifdef NO_TRANSLATION_SERVICE
    return CreateStubEndPoint();
#else
    return CreateMaybeStubEndPoint();
#endif
}

} // namespace RTC
