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

Translator::Translator(const Producer* producer,
                       const WebsocketFactory* websocketFactory,
                       RtpPacketsPlayer* rtpPacketsPlayer,
                       RtpPacketsCollector* output,
                       const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<TranslatorEndPointFactory>(allocator)
    , _producer(producer)
#ifndef NO_TRANSLATION_SERVICE
    , _websocketFactory(websocketFactory)
#endif
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
    LOCK_WRITE_PROTECTED_OBJ(_originalSsrcToStreams);
    _originalSsrcToStreams->clear();
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
            LOCK_WRITE_PROTECTED_OBJ(_originalSsrcToStreams);
            const auto it = _originalSsrcToStreams->find(stream->GetSsrc());
            if (it == _originalSsrcToStreams->end()) {
                auto source = TranslatorSource::Create(stream, mappedSsrc, this,
                                                       _rtpPacketsPlayer,  _output,
                                                       _producer->id, GetAllocator());
                if (source) {
                    source->SetInputLanguage(_producer->GetLanguageId());
                    AddConsumersToSource(source.get());
                    _originalSsrcToStreams->insert({stream->GetSsrc(), std::move(source)});
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
        LOCK_WRITE_PROTECTED_OBJ(_originalSsrcToStreams);
        auto it = _originalSsrcToStreams->find(ssrc);
        if (it == _originalSsrcToStreams->end()) {
            // maybe SSRC is mangled
            const auto itm = _mappedSsrcToOriginal.find(ssrc);
            if (itm != _mappedSsrcToOriginal.end()) {
                ssrc = itm->second;
                _mappedSsrcToOriginal.erase(itm);
                it = _originalSsrcToStreams->find(ssrc);
            }
        }
        if (it != _originalSsrcToStreams->end()) {
            _originalSsrcToStreams->erase(ssrc);
            return true;
        }
    }
    return false;
}

bool Translator::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    if (packet && !_producer->IsPaused()) {
        if (const auto ssrc = packet->GetSsrc()) {
            LOCK_READ_PROTECTED_OBJ(_originalSsrcToStreams);
            auto it = _originalSsrcToStreams->find(ssrc);
            if (it == _originalSsrcToStreams->end()) {
                // maybe SSRC is mangled
                const auto itm = _mappedSsrcToOriginal.find(ssrc);
                if (itm != _mappedSsrcToOriginal.end()) {
                    it = _originalSsrcToStreams->find(itm->second);
                }
            }
            if (it != _originalSsrcToStreams->end()) {
                return it->second->AddOriginalRtpPacketForTranslation(packet);
            }
        }
    }
    return false;
}

const std::string& Translator::GetId() const
{
    return _producer->id;
}

void Translator::AddConsumer(Consumer* consumer)
{
    if (consumer && Media::Kind::AUDIO == consumer->GetKind()) {
        MS_ASSERT(consumer->producerId == GetId(), "wrong producer ID");
        LOCK_WRITE_PROTECTED_OBJ(_consumers);
        if (_consumers->end() == std::find(_consumers->begin(), _consumers->end(), consumer)) {
            LOCK_READ_PROTECTED_OBJ(_originalSsrcToStreams);
            for (auto it = _originalSsrcToStreams->begin(); it != _originalSsrcToStreams->end(); ++it) {
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
            LOCK_READ_PROTECTED_OBJ(_originalSsrcToStreams);
            for (auto it = _originalSsrcToStreams->begin(); it != _originalSsrcToStreams->end(); ++it) {
                it->second->RemoveConsumer(consumer);
            }
            _consumers->erase(it);
        }
    }
}

void Translator::UpdateProducerLanguage()
{
    LOCK_READ_PROTECTED_OBJ(_originalSsrcToStreams);
    for (auto it = _originalSsrcToStreams->begin(); it != _originalSsrcToStreams->end(); ++it) {
        it->second->SetInputLanguage(_producer->GetLanguageId());
    }
}

void Translator::UpdateConsumerLanguageOrVoice(Consumer* consumer)
{
    if (consumer && Media::Kind::AUDIO == consumer->GetKind()) {
        LOCK_READ_PROTECTED_OBJ(_originalSsrcToStreams);
        for (auto it = _originalSsrcToStreams->begin(); it != _originalSsrcToStreams->end(); ++it) {
            it->second->UpdateConsumer(consumer);
        }
    }
}

void Translator::AddConsumersToSource(TranslatorSource* source) const
{
    if (source) {
        LOCK_READ_PROTECTED_OBJ(_consumers);
        for (const auto consumer : _consumers.ConstRef()) {
            source->AddConsumer(consumer);
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
    return std::make_shared<StubEndPoint>(GetId(), _rtpPacketsPlayer->GetTimer());
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
    return std::make_shared<StubEndPoint>(GetId(), _rtpPacketsPlayer->GetTimer());
}

#else

std::shared_ptr<TranslatorEndPoint> Translator::CreateMaybeStubEndPoint() const
{
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    if (0 == WebsocketEndPoint::GetInstancesCount()) {
        const auto socketEndPoint = WebsocketEndPoint::Create(_websocketFactory, GetId());
        if (socketEndPoint) {
            _nonStubEndPointRef = socketEndPoint;
            return socketEndPoint;
        }
    }
    return std::make_shared<StubEndPoint>(GetId(), _rtpPacketsPlayer->GetTimer());
#else
    return WebsocketEndPoint::Create(_websocketFactory, GetId());
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
