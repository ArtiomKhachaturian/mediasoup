#define MS_CLASS "RTC::Translator"
#include "RTC/MediaTranslate/Translator.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/TranslatorSource.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
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

class Translator::ConsumerTranslatorImpl : public ConsumerTranslator
{
public:
    ConsumerTranslatorImpl(uint64_t id, std::string languageId, std::string voiceId);
    // return true if changed
    bool SetLanguageId(std::string languageId);
    bool SetVoiceId(std::string voiceId);
    // impl. of ConsumerTranslator
    uint64_t GetId() const final { return _id; }
    std::string GetLanguageId() const final { return _languageId; }
    std::string GetVoiceId() const final { return _voiceId; }
private:
    const uint64_t _id;
    std::string _languageId;
    std::string _voiceId;
};

Translator::Translator(const Producer* producer,
                       const WebsocketFactory* websocketFactory,
                       RtpPacketsPlayer* rtpPacketsPlayer,
                       RtpPacketsCollector* output,
                       const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<TranslatorEndPointFactory>(allocator)
    , _producerId(producer->id)
#ifndef NO_TRANSLATION_SERVICE
    , _websocketFactory(websocketFactory)
#endif
    , _rtpPacketsPlayer(rtpPacketsPlayer)
    , _output(output)
    , _producerPaused(producer->IsPaused())
    , _producerLanguageId(producer->GetLanguageId())
{
}

Translator::~Translator()
{
    _consumers.clear();
    _originalSsrcToStreams.clear();
    _mappedSsrcToOriginal.clear();
}

std::unique_ptr<Translator> Translator::Create(const Producer* producer,
                                               const WebsocketFactory* websocketFactory,
                                               RtpPacketsPlayer* rtpPacketsPlayer,
                                               RtpPacketsCollector* output,
                                               const std::shared_ptr<BufferAllocator>& allocator)
{
    if (producer && rtpPacketsPlayer && output && Media::Kind::AUDIO == producer->GetKind()) {
        auto translator = new Translator(producer, websocketFactory, rtpPacketsPlayer,
                                         output, allocator);
        // add streams
        const auto& streams = producer->GetRtpStreams();
        for (auto it = streams.begin(); it != streams.end(); ++it) {
            translator->AddStream(it->first, it->second);
        }
        return std::unique_ptr<Translator>(translator);
    }
    return nullptr;
}

bool Translator::AddStream(const RtpStream* stream, uint32_t mappedSsrc)
{
    bool ok = false;
    if (stream && mappedSsrc) {
        const auto& mime = stream->GetMimeType();
        if (mime.IsAudioCodec()) {
            const auto it = _originalSsrcToStreams.find(stream->GetSsrc());
            if (it == _originalSsrcToStreams.end()) {
                auto source = TranslatorSource::Create(stream, mappedSsrc, this,
                                                       _rtpPacketsPlayer,  _output,
                                                       GetId(), GetAllocator());
                if (source) {
                    source->SetInputLanguage(GetProducerLanguageId());
                    source->SetPaused(_producerPaused);
                    AddConsumersToSource(source.get());
                    _originalSsrcToStreams.insert({stream->GetSsrc(), std::move(source)});
                    ok = true;
                }
                else {
                    const auto desc = GetStreamInfoString(mime, stream->GetSsrc());
                    MS_ERROR_STD("depacketizer or serializer is not available for stream [%s]", desc.c_str());
                }
            }
            else {
                MS_ASSERT(it->second->GetMime() == stream->GetMimeType(), "MIME type mistmatch");
                MS_ASSERT(it->second->GetClockRate() == stream->GetClockRate(), "clock rate mistmatch");
                MS_ASSERT(it->second->GetPayloadType() == stream->GetPayloadType(), "payload type mistmatch");
                MS_ASSERT(it->second->GetMappedSsrc() == mappedSsrc, "mapped SSRC mistmatch");
                ok = true; // already registered
            }
            if (ok) {
                _mappedSsrcToOriginal[stream->GetSsrc()] = mappedSsrc;
            }
        }
    }
    return ok;
}

bool Translator::RemoveStream(uint32_t ssrc)
{
    if (ssrc) {
        auto it = _originalSsrcToStreams.find(ssrc);
        if (it == _originalSsrcToStreams.end()) {
            // maybe SSRC is mangled
            const auto itm = _mappedSsrcToOriginal.find(ssrc);
            if (itm != _mappedSsrcToOriginal.end()) {
                ssrc = itm->second;
                _mappedSsrcToOriginal.erase(itm);
                it = _originalSsrcToStreams.find(ssrc);
            }
        }
        if (it != _originalSsrcToStreams.end()) {
            _originalSsrcToStreams.erase(ssrc);
            return true;
        }
    }
    return false;
}

void Translator::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    if (packet) {
        if (const auto ssrc = packet->GetSsrc()) {
            auto it = _originalSsrcToStreams.find(ssrc);
            if (it == _originalSsrcToStreams.end()) {
                // maybe SSRC is mangled
                const auto itm = _mappedSsrcToOriginal.find(ssrc);
                if (itm != _mappedSsrcToOriginal.end()) {
                    it = _originalSsrcToStreams.find(itm->second);
                }
            }
            if (it != _originalSsrcToStreams.end()) {
                it->second->AddOriginalRtpPacketForTranslation(packet);
            }
        }
    }
}

void Translator::AddConsumer(const Consumer* consumer)
{
    if (consumer && Media::Kind::AUDIO == consumer->GetKind()) {
        MS_ASSERT(consumer->producerId == GetId(), "wrong producer ID");
        const auto id = consumer->GetId();
        if (!_consumers.count(id)) {
            auto translator = std::make_shared<ConsumerTranslatorImpl>(id, consumer->GetLanguageId(),
                                                                       consumer->GetVoiceId());
            for (auto it = _originalSsrcToStreams.begin();
                 it != _originalSsrcToStreams.end(); ++it) {
                it->second->AddConsumer(translator);
            }
            _consumers.insert(std::make_pair(id, std::move(translator)));
        }
    }
}

void Translator::RemoveConsumer(const Consumer* consumer)
{
    if (consumer) {
        const auto itc = _consumers.find(consumer->GetId());
        if (itc != _consumers.end()) {
            for (auto it = _originalSsrcToStreams.begin();
                 it != _originalSsrcToStreams.end(); ++it) {
                it->second->RemoveConsumer(itc->second);
            }
            _consumers.erase(itc);
        }
    }
}

void Translator::SetProducerPaused(bool paused)
{
    if (_producerPaused != paused) {
        _producerPaused = paused;
        for (auto it = _originalSsrcToStreams.begin(); it != _originalSsrcToStreams.end(); ++it) {
            it->second->SetPaused(paused);
        }
    }
}

void Translator::SetProducerLanguageId(std::string languageId)
{
    if (_producerLanguageId != languageId) {
        _producerLanguageId = std::move(languageId);
        for (auto it = _originalSsrcToStreams.begin(); it != _originalSsrcToStreams.end(); ++it) {
            it->second->SetInputLanguage(GetProducerLanguageId());
        }
    }
}

void Translator::UpdateConsumerLanguageOrVoice(const Consumer* consumer)
{
    if (consumer) {
        const auto itc = _consumers.find(consumer->GetId());
        if (itc != _consumers.end()) {
            const auto& translator = itc->second;
            if (translator->SetLanguageId(consumer->GetLanguageId()) ||
                translator->SetVoiceId(consumer->GetVoiceId())) {
                for (auto it = _originalSsrcToStreams.begin();
                     it != _originalSsrcToStreams.end(); ++it) {
                    it->second->UpdateConsumer(translator);
                }
            }
        }
    }
}

void Translator::AddConsumersToSource(TranslatorSource* source) const
{
    if (source) {
        for (auto it = _consumers.begin(); it != _consumers.end(); ++it) {
            source->AddConsumer(it->second);
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
    return std::make_shared<StubEndPoint>(GetId(), GetAllocator(), _rtpPacketsPlayer->GetTimer());
#else
    return CreateMaybeFileEndPoint();
#endif
}

std::shared_ptr<TranslatorEndPoint> Translator::CreateMaybeFileEndPoint() const
{

    auto fileEndPoint = std::make_shared<FileEndPoint>(GetId(),
                                                       GetAllocator(),
                                                       _rtpPacketsPlayer->GetTimer());
    if (!fileEndPoint->IsValid()) {
        MS_ERROR_STD("failed open [%s] as mock translation", fileEndPoint->GetName().c_str());
    }
    else {
        _nonStubEndPointRef = fileEndPoint;
        return fileEndPoint;
    }
    return std::make_shared<StubEndPoint>(GetId(), GetAllocator(), _rtpPacketsPlayer->GetTimer());
}

#else

std::shared_ptr<TranslatorEndPoint> Translator::CreateMaybeStubEndPoint() const
{
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    if (0 == WebsocketEndPoint::GetInstancesCount()) {
        const auto socketEndPoint = WebsocketEndPoint::Create(_websocketFactory,
                                                              GetId(), GetAllocator());
        if (socketEndPoint) {
            _nonStubEndPointRef = socketEndPoint;
            return socketEndPoint;
        }
    }
    return std::make_shared<StubEndPoint>(GetId(), GetAllocator(), _rtpPacketsPlayer->GetTimer());
#else
    return WebsocketEndPoint::Create(_websocketFactory, GetId(), GetAllocator());
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

Translator::ConsumerTranslatorImpl::ConsumerTranslatorImpl(uint64_t id,
                                                           std::string languageId,
                                                           std::string voiceId)
    : _id(id)
    , _languageId(std::move(languageId))
    , _voiceId(std::move(voiceId))
{
}

bool Translator::ConsumerTranslatorImpl::SetLanguageId(std::string languageId)
{
    if (_languageId != languageId) {
        _languageId = std::move(languageId);
        return true;
    }
    return false;
}

bool Translator::ConsumerTranslatorImpl::SetVoiceId(std::string voiceId)
{
    if (_voiceId != voiceId) {
        _voiceId = std::move(voiceId);
        return true;
    }
    return false;
}

} // namespace RTC
