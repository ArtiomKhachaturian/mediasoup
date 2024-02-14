#define MS_CLASS "RTC::Translator"
#include "RTC/MediaTranslate/Translator.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/ConsumersManager.hpp"
#include "RTC/MediaTranslate/ConsumerInfo.hpp"
#if defined(NO_TRANSLATION_SERVICE) || defined(SINGLE_TRANSLATION_POINT_CONNECTION)
#include "RTC/MediaTranslate/TranslatorEndPoint/MockEndPoint.hpp"
#endif
#ifndef NO_TRANSLATION_SERVICE
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketEndPoint.hpp"
#endif
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#ifdef READ_PRODUCER_RECV_FROM_FILE
#include "RTC/MediaTranslate/FileReader.hpp"
#endif
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/RtpStream.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"


namespace {

#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
std::atomic<uint64_t> g_InstancesCounter = 0ULL;
#endif

}

namespace RTC
{
class Translator::SourceStream
{
public:
    SourceStream(uint32_t clockRate, uint8_t payloadType,
                 std::unique_ptr<MediaFrameSerializer> serializer,
                 std::unique_ptr<RtpDepacketizer> depacketizer,
                 const std::string& producerId,
                 TranslatorEndPointFactory* endPointsFactory,
                 MediaSource* translationsInput,
                 TranslatorEndPointListener* translationsOutput);
    ~SourceStream();
    static std::shared_ptr<SourceStream> Create(const RtpCodecMimeType& mime,
                                                uint32_t clockRate,
                                                uint32_t originalSsrc,
                                                uint8_t payloadType,
                                                const std::string& producerId,
                                                TranslatorEndPointFactory* endPointsFactory,
                                                MediaSource* translationsInput,
                                                TranslatorEndPointListener* translationsOutput);
    uint32_t GetClockRate() const { return _depacketizer->GetClockRate(); }
    uint8_t GetPayloadType() const { return _payloadType; }
    const RtpCodecMimeType& GetMime() const { return _depacketizer->GetMimeType(); }
    uint32_t GetOriginalSsrc() const { return _serializer->GetSsrc(); }
    uint64_t GetAddedPacketsCount() const { return _addedPacketsCount; }
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    void IncreaseAddedPacketsCount() { ++_addedPacketsCount; }
#endif
    void AddSink(MediaSink* sink);
    void RemoveSink(MediaSink* sink);
    void RemoveAllSinks();
    bool AddOriginalRtpPacketForTranslation(RtpPacket* packet);
    void SetInputLanguage(const std::string& languageId);
    void AddConsumer(Consumer* consumer);
    void UpdateConsumer(const Consumer* consumer);
    void RemoveConsumer(Consumer* consumer);
    bool IsConnected(const Consumer* consumer) const;
    void SaveProducerRtpPacketInfo(const Consumer* consumer, const RtpPacket* packet);
    void AlignProducerRtpPacketInfo(const Consumer* consumer, RtpPacket* packet);
    void PlayTranslatedPacket(const TranslatorEndPoint* from, uint32_t rtpTimestampOffset,
                              RtpPacket* packet, RtpPacketsCollector* output);
private:
#ifdef READ_PRODUCER_RECV_FROM_FILE
    static std::unique_ptr<FileReader> CreateFileReader(uint32_t ssrc);
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string& producerId,
                                                        const std::string_view& fileExtension);
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string& producerId,
                                                        const MediaFrameSerializer* serializer);
#endif
private:
#ifdef READ_PRODUCER_RECV_FROM_FILE
    //static inline const char* _testFileName = "/Users/user/Downloads/1b0cefc4-abdb-48d0-9c50-f5050755be94.webm";
    //static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/producer_test.webm";
    static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm";
#endif
    const uint8_t _payloadType = 0U;
    const std::unique_ptr<MediaFrameSerializer> _serializer;
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
#ifdef READ_PRODUCER_RECV_FROM_FILE
    const std::unique_ptr<FileReader> _fileReader;
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    std::unique_ptr<FileWriter> _fileWriter;
#endif
    ProtectedObj<ConsumersManager> _consumersManager;
    absl::flat_hash_map<Consumer*, std::shared_ptr<ConsumerInfo>> _consumers;
    uint64_t _addedPacketsCount = 0ULL;
};

Translator::Translator(const Producer* producer, RtpPacketsPlayer* rtpPacketsPlayer,
                       RtpPacketsCollector* output)
    : _producer(producer)
    , _rtpPacketsPlayer(rtpPacketsPlayer)
    , _output(output)
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    , _instanceIndex(g_InstancesCounter.fetch_add(1U))
#endif
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
    RemoveAllSinks();
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    g_InstancesCounter.fetch_sub(1U);
#endif
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
                auto sourceStream = SourceStream::Create(mime, clockRate,
                                                         originalSsrc, payloadType,
                                                         _producer->id,
                                                         this, this, this);
                if (sourceStream) {
                    sourceStream->SetInputLanguage(_producer->GetLanguageId());
                    AddSinksToStream(sourceStream);
                    AddConsumersToStream(sourceStream);
                    _rtpPacketsPlayer->AddStream(originalSsrc, mime, this,  this);
                    _originalSsrcToStreams[originalSsrc] = sourceStream;
                    _mappedSsrcToStreams->insert({mappedSsrc, std::move(sourceStream)});
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
            _rtpPacketsPlayer->RemoveStream(it->second->GetOriginalSsrc());
            _originalSsrcToStreams.erase(it->second->GetOriginalSsrc());
            _mappedSsrcToStreams->erase(it);
            return true;
        }
    }
    return false;
}

void Translator::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    if (packet && !_producer->IsPaused()) {
        if (const auto stream = GetStream(packet->GetSsrc())) {
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
            const auto added = _instanceIndex > 0U || stream->AddOriginalRtpPacketForTranslation(packet);
            if (added && _instanceIndex > 0U) { // fake
                stream->IncreaseAddedPacketsCount();
            }
#else
            const auto added = stream->AddOriginalRtpPacketForTranslation(packet);
#endif
            PostProcessAfterAdding(packet, added, stream);
        }
    }
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

std::shared_ptr<Translator::SourceStream> Translator::GetStream(uint32_t ssrc) const
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

void Translator::AddSinksToStream(const std::shared_ptr<SourceStream>& stream) const
{
    if (stream) {
        LOCK_READ_PROTECTED_OBJ(_sinks);
        for (const auto sink : _sinks.ConstRef()) {
            stream->AddSink(sink);
        }
    }
}

void Translator::AddConsumersToStream(const std::shared_ptr<SourceStream>& stream) const
{
    if (stream) {
        LOCK_READ_PROTECTED_OBJ(_consumers);
        for (const auto consumer : _consumers.ConstRef()) {
            stream->AddConsumer(consumer);
        }
    }
}

void Translator::PostProcessAfterAdding(RtpPacket* packet, bool added,
                                        const std::shared_ptr<SourceStream>& stream)
{
    if (packet && stream) {
        LOCK_READ_PROTECTED_OBJ(_consumers);
        if (!_consumers->empty()) {
            const auto ssrc = stream->GetOriginalSsrc();
            const auto initial = added && 2UL == stream->GetAddedPacketsCount();
            for (const auto consumer : _consumers.ConstRef()) {
                const auto rejected = added && (stream->IsConnected(consumer) || _rtpPacketsPlayer->IsPlaying(ssrc));
                if (initial) {
                    stream->SaveProducerRtpPacketInfo(consumer, packet);
                }
                if (rejected) {
                    packet->AddRejectedConsumer(consumer);
                }
                else {
                    stream->AlignProducerRtpPacketInfo(consumer, packet);
                }
            }
        }
    }
}

bool Translator::AddSink(MediaSink* sink)
{
    if (sink) {
        LOCK_WRITE_PROTECTED_OBJ(_sinks);
        if (_sinks->end() == std::find(_sinks->begin(), _sinks->end(), sink)) {
            _sinks->push_back(sink);
            LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
            for (auto it = _mappedSsrcToStreams->begin();
                 it != _mappedSsrcToStreams->end(); ++it) {
                it->second->AddSink(sink);
            }
            return true;
        }
    }
    return false;
}

bool Translator::RemoveSink(MediaSink* sink)
{
    if (sink) {
        LOCK_WRITE_PROTECTED_OBJ(_sinks);
        const auto it = std::find(_sinks->begin(), _sinks->end(), sink);
        if (it != _sinks->end()) {
            _sinks->erase(it);
            LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
            for (auto it = _mappedSsrcToStreams->begin();
                 it != _mappedSsrcToStreams->end(); ++it) {
                it->second->RemoveSink(sink);
            }
            return true;
        }
    }
    return false;
}

void Translator::RemoveAllSinks()
{
    LOCK_WRITE_PROTECTED_OBJ(_sinks);
    LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
    for (auto it = _mappedSsrcToStreams->begin();
         it != _mappedSsrcToStreams->end(); ++it) {
        it->second->RemoveAllSinks();
    }
    _sinks->clear();
}

bool Translator::HasSinks() const
{
    LOCK_READ_PROTECTED_OBJ(_sinks);
    return !_sinks->empty();
}

size_t Translator::GetSinksCout() const
{
    LOCK_READ_PROTECTED_OBJ(_sinks);
    return _sinks.ConstRef().size();
}
    
void Translator::OnTranslatedMediaReceived(const TranslatorEndPoint* endPoint,
                                           uint64_t mediaSeqNum,
                                           const std::shared_ptr<MemoryBuffer>& media)
{
    if (endPoint && media) {
        MS_ERROR_STD("Received translation #%llu", mediaSeqNum);
        _rtpPacketsPlayer->Play(endPoint->GetSsrc(), mediaSeqNum, media, endPoint);
    }
}

void Translator::OnPlayStarted(uint32_t ssrc, uint64_t mediaId, const void* userData)
{
    RtpPacketsPlayerCallback::OnPlayStarted(ssrc, mediaId, userData);
    //MS_ERROR_STD("Translation #%llu play was started", mediaId);
}

void Translator::OnPlay(uint32_t rtpTimestampOffset, RtpPacket* packet,
                        uint64_t mediaId, const void* userData)
{
    if (packet && userData) {
        if (const auto stream = GetStream(packet->GetSsrc())) {
            const auto endPoint = reinterpret_cast<const TranslatorEndPoint*>(userData);
            stream->PlayTranslatedPacket(endPoint, rtpTimestampOffset, packet, _output);
        }
    }
}

void Translator::OnPlayFinished(uint32_t ssrc, uint64_t mediaId, const void* userData)
{
    RtpPacketsPlayerCallback::OnPlayFinished(ssrc, mediaId, userData);
    //MS_ERROR_STD("Translation #%llu play was finished", mediaId);
}

std::shared_ptr<TranslatorEndPoint> Translator::CreateEndPoint(uint32_t ssrc)
{
    std::shared_ptr<TranslatorEndPoint> endPoint;
#ifdef NO_TRANSLATION_SERVICE
    /*endPoint = std::make_shared<MockEndPoint>(ssrc, _mockTranslationFileName,
                                              _mockTranslationFileNameLenMs + 1000U);*/
    endPoint = std::make_shared<MockEndPoint>(ssrc);
#else
    endPoint = std::make_shared<WebsocketEndPoint>(ssrc);
#endif
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    if (_instanceIndex) {
        endPoint = std::make_shared<MockEndPoint>(ssrc);
    }
    else {
        // TODO: maybe deadlock when shared or regular mutex, potential conflict in Translator::AddConsumer
        static_assert(std::is_same<std::recursive_mutex, ProtectedObj<std::list<Consumer*>>::ObjectMutexType>::value);
        LOCK_READ_PROTECTED_OBJ(_consumers);
        if (!_consumers->empty()) {
            endPoint = std::make_shared<MockEndPoint>(ssrc);
        }
    }
#endif
    return endPoint;
}

uint8_t Translator::GetPayloadType(uint32_t ssrc) const
{
    if (const auto stream = GetStream(ssrc)) {
        return stream->GetPayloadType();
    }
    return 0U;
}

uint32_t Translator::GetClockRate(uint32_t ssrc) const
{
    if (const auto stream = GetStream(ssrc)) {
        return stream->GetClockRate();
    }
    return 0U;
}

Translator::SourceStream::SourceStream(uint32_t clockRate, uint8_t payloadType,
                                       std::unique_ptr<MediaFrameSerializer> serializer,
                                       std::unique_ptr<RtpDepacketizer> depacketizer,
                                       const std::string& producerId,
                                       TranslatorEndPointFactory* endPointsFactory,
                                       MediaSource* translationsInput,
                                       TranslatorEndPointListener* translationsOutput)
    : _payloadType(payloadType)
    , _serializer(std::move(serializer))
    , _depacketizer(std::move(depacketizer))
#ifdef READ_PRODUCER_RECV_FROM_FILE
    , _fileReader(CreateFileReader(_serializer->GetSsrc()))
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    , _fileWriter(CreateFileWriter(_serializer->GetSsrc(), producerId, _serializer.get()))
#endif
    , _consumersManager(_serializer->GetSsrc(), endPointsFactory, translationsInput, translationsOutput)
{
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter && !_serializer->AddTestSink(_fileWriter.get())) {
        // TODO: log error
        _fileWriter->DeleteFromStorage();
        _fileWriter.reset();
    }
#endif
}

Translator::SourceStream::~SourceStream()
{
    RemoveAllSinks();
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter) {
        _serializer->RemoveTestSink();
    }
#endif
}

std::shared_ptr<Translator::SourceStream> Translator::SourceStream::Create(const RtpCodecMimeType& mime,
                                                                           uint32_t clockRate,
                                                                           uint32_t originalSsrc,
                                                                           uint8_t payloadType,
                                                                           const std::string& producerId,
                                                                           TranslatorEndPointFactory* endPointsFactory,
                                                                           MediaSource* translationsInput,
                                                                           TranslatorEndPointListener* translationsOutput)
{
    MS_ASSERT(WebMCodecs::IsSupported(mime), "WebM not available for this MIME %s", mime.ToString().c_str());
    std::shared_ptr<SourceStream> stream;
    if (auto depacketizer = RtpDepacketizer::Create(mime, clockRate)) {
        auto serializer = std::make_unique<WebMSerializer>(originalSsrc, mime);
        stream = std::make_shared<SourceStream>(clockRate, payloadType,
                                                std::move(serializer),
                                                std::move(depacketizer),
                                                producerId,
                                                endPointsFactory,
                                                translationsInput,
                                                translationsOutput);
    }
    else {
        MS_ERROR_STD("failed to create depacketizer, MIME type %s", mime.ToString().c_str());
    }
    return stream;
}

void Translator::SourceStream::AddSink(MediaSink* sink)
{
    if (sink) {
        MediaSource* source = _serializer.get();
#ifdef READ_PRODUCER_RECV_FROM_FILE
        if (_fileReader) {
            source = _fileReader.get();
        }
#endif
        source->AddSink(sink);
    }
}

void Translator::SourceStream::RemoveSink(MediaSink* sink)
{
    if (sink) {
        MediaSource* source = _serializer.get();
#ifdef READ_PRODUCER_RECV_FROM_FILE
        if (_fileReader) {
            source = _fileReader.get();
        }
#endif
        source->RemoveSink(sink);
    }
}

void Translator::SourceStream::RemoveAllSinks()
{
    MediaSource* source = _serializer.get();
#ifdef READ_PRODUCER_RECV_FROM_FILE
    if (_fileReader) {
        source = _fileReader.get();
    }
#endif
    source->RemoveAllSinks();
}

bool Translator::SourceStream::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    bool handled = false;
    if (packet) {
#ifdef READ_PRODUCER_RECV_FROM_FILE
        if (_fileReader) {
            handled = _serializer->HasSinks();
            if (handled) {
                ++_addedPacketsCount;
            }
            return handled;
        }
#endif
        if (const auto frame = _depacketizer->AddPacket(packet)) {
            handled = _serializer->Push(frame);
        }
        else if (GetMime().IsAudioCodec() && _serializer->HasSinks()) {
            // maybe empty packet if silence
            handled = nullptr == packet->GetPayload() || 0U == packet->GetPayloadLength();
        }
    }
    if (handled) {
        ++_addedPacketsCount;
    }
    return handled;
}

void Translator::SourceStream::SetInputLanguage(const std::string& languageId)
{
    LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
    _consumersManager->SetInputLanguage(languageId);
}

void Translator::SourceStream::AddConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
        if (auto info = _consumersManager->AddConsumer(consumer)) {
            _consumers[consumer] = std::move(info);
        }
    }
}

void Translator::SourceStream::UpdateConsumer(const Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
        _consumersManager->UpdateConsumer(consumer);
    }
}

void Translator::SourceStream::RemoveConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
        if (_consumersManager->RemoveConsumer(consumer)) {
            _consumers.erase(consumer);
        }
    }
}

bool Translator::SourceStream::IsConnected(const Consumer* consumer) const
{
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_consumersManager);
        const auto it = _consumers.find(consumer);
        return it != _consumers.end() && it->second->IsConnected();
    }
    return false;
}

void Translator::SourceStream::SaveProducerRtpPacketInfo(const Consumer* consumer,
                                                         const RtpPacket* packet)
{
    if (consumer && packet) {
        LOCK_READ_PROTECTED_OBJ(_consumersManager);
        const auto it = _consumers.find(consumer);
        if (it != _consumers.end()) {
            it->second->SaveProducerRtpPacketInfo(packet);
        }
    }
}

void Translator::SourceStream::AlignProducerRtpPacketInfo(const Consumer* consumer,
                                                          RtpPacket* packet)
{
    if (consumer && packet) {
        LOCK_READ_PROTECTED_OBJ(_consumersManager);
        const auto it = _consumers.find(consumer);
        if (it != _consumers.end()) {
            it->second->AlignProducerRtpPacketInfo(packet);
        }
    }
}

void Translator::SourceStream::PlayTranslatedPacket(const TranslatorEndPoint* from,
                                                    uint32_t rtpTimestampOffset,
                                                    RtpPacket* packet,
                                                    RtpPacketsCollector* output)
{
    if (from && packet) {
        LOCK_READ_PROTECTED_OBJ(_consumersManager);
        if (!_consumers.empty()) {
            std::shared_ptr<RTC::RtpPacket> sharedPacket;
            for (auto it = _consumers.begin(); it != _consumers.end(); ++it) {
                if (it->second->GetEndPoint().get() == from) {
                    it->second->AlignTranslatedRtpPacketInfo(rtpTimestampOffset, packet);
                    output->AddPacket(packet);
                }
                else {
                    packet->AddRejectedConsumer(it->first);
                }
            }
        }
    }
}

#ifdef READ_PRODUCER_RECV_FROM_FILE
std::unique_ptr<FileReader> Translator::SourceStream::CreateFileReader(uint32_t ssrc)
{
    auto fileReader = std::make_unique<FileReader>(_testFileName, ssrc, false);
    if (!fileReader->IsOpen()) {
        MS_WARN_DEV_STD("Failed to open producer file input %s", _testFileName);
        fileReader.reset();
    }
    return fileReader;
}
#endif

#ifdef WRITE_PRODUCER_RECV_TO_FILE
std::unique_ptr<FileWriter> Translator::SourceStream::CreateFileWriter(uint32_t ssrc,
                                                                       const std::string& producerId,
                                                                       const std::string_view& fileExtension)
{
    if (!fileExtension.empty()) {
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = producerId + "_" + std::to_string(ssrc) + "." + std::string(fileExtension);
            fileName = std::string(depacketizerPath) + "/" + fileName;
            auto fileWriter = std::make_unique<FileWriter>(fileName);
            if (!fileWriter->IsOpen()) {
                fileWriter.reset();
                MS_WARN_DEV_STD("Failed to open producer file output %s", fileName.c_str());
            }
            return fileWriter;
        }
    }
    return nullptr;
}

std::unique_ptr<FileWriter> Translator::SourceStream::CreateFileWriter(uint32_t ssrc,
                                                                       const std::string& producerId,
                                                                       const MediaFrameSerializer* serializer)
{
    if (serializer) {
        return CreateFileWriter(ssrc, producerId, serializer->GetFileExtension());
    }
    return nullptr;
}
#endif

} // namespace RTC
