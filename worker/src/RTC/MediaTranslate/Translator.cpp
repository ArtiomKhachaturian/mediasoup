#define MS_CLASS "RTC::Translator"
#include "RTC/MediaTranslate/Translator.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
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
#include "RTC/MediaTranslate/TranslatorEndPoint/StubEndPoint.hpp"
#endif
#ifdef NO_TRANSLATION_SERVICE
#include "RTC/MediaTranslate/TranslatorEndPoint/FileEndPoint.hpp"
#else
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketEndPoint.hpp"
#endif
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#ifdef READ_PRODUCER_RECV_FROM_FILE
#include "RTC/MediaTranslate/FileReader.hpp"
#endif
#include "RTC/RtpStream.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"


namespace RTC
{
class Translator::SourceStream : private RtpPacketsPlayerCallback,
                                 private MediaSink // receiver of translated audio packets
{
public:
    SourceStream(uint32_t clockRate, uint32_t originalSsrc, uint8_t payloadType,
                 std::unique_ptr<MediaFrameSerializer> serializer,
                 std::unique_ptr<RtpDepacketizer> depacketizer,
                 TranslatorEndPointFactory* endPointsFactory,
                 RtpPacketsPlayer* rtpPacketsPlayer,
                 RtpPacketsCollector* output,
                 const std::string& producerId);
    ~SourceStream();
    static std::shared_ptr<SourceStream> Create(const RtpCodecMimeType& mime,
                                                uint32_t clockRate,
                                                uint32_t originalSsrc,
                                                uint8_t payloadType,
                                                TranslatorEndPointFactory* endPointsFactory,
                                                RtpPacketsPlayer* rtpPacketsPlayer,
                                                RtpPacketsCollector* output,
                                                const std::string& producerId);
    const RtpCodecMimeType& GetMime() const { return _depacketizer->GetMimeType(); }
    uint32_t GetClockRate() const { return _depacketizer->GetClockRate(); }
     uint8_t GetPayloadType() const { return _payloadType; }
    uint32_t GetOriginalSsrc() const { return _originalSsrc; }
    uint64_t GetAddedPacketsCount() const { return _addedPacketsCount; }
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    void IncreaseAddedPacketsCount() { ++_addedPacketsCount; }
#endif
    bool AddOriginalRtpPacketForTranslation(RtpPacket* packet);
    void SetInputLanguage(const std::string& languageId);
    void AddConsumer(Consumer* consumer);
    void UpdateConsumer(Consumer* consumer);
    void RemoveConsumer(Consumer* consumer);
    bool IsConnected(Consumer* consumer) const;
    void SaveProducerRtpPacketInfo(Consumer* consumer, const RtpPacket* packet);
private:
#ifdef READ_PRODUCER_RECV_FROM_FILE
    static std::unique_ptr<FileReader> CreateFileReader();
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string& producerId,
                                                        const std::string_view& fileExtension);
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string& producerId,
                                                        const MediaFrameSerializer* serializer);
#endif
    // source of original audio packets, maybe mock (audio file) if READ_PRODUCER_RECV_FROM_FILE defined
    MediaSource* GetMediaSource() const;
    MediaSink* GetMediaReceiver() { return this; }
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId);
    void OnPlay(const Timestamp& timestampOffset, RtpPacket* packet,
                uint64_t mediaId, uint64_t mediaSourceId) final;
    void OnPlayFinished(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
    // impl. of MediaSink
    void WriteMediaPayload(const MediaObject& sender, const std::shared_ptr<MemoryBuffer>& buffer) final;
private:
#ifdef READ_PRODUCER_RECV_FROM_FILE
    //static inline const char* _testFileName = "/Users/user/Downloads/1b0cefc4-abdb-48d0-9c50-f5050755be94.webm";
    //static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/producer_test.webm";
    static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm";
#endif
    const uint32_t _originalSsrc;
    const uint8_t _payloadType;
    const std::unique_ptr<MediaFrameSerializer> _serializer;
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    RtpPacketsPlayer* const _rtpPacketsPlayer;
    RtpPacketsCollector* const _output;
#ifdef READ_PRODUCER_RECV_FROM_FILE
    const std::unique_ptr<FileReader> _fileReader;
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    std::unique_ptr<FileWriter> _fileWriter;
#endif
    ProtectedObj<ConsumersManager> _consumersManager;
    uint64_t _addedPacketsCount = 0ULL;
};

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
                auto sourceStream = SourceStream::Create(mime, clockRate, originalSsrc,
                                                         payloadType, this, _rtpPacketsPlayer,
                                                         _output, _producer->id);
                if (sourceStream) {
                    sourceStream->SetInputLanguage(_producer->GetLanguageId());
                    AddConsumersToStream(sourceStream);
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
            const auto added = stream->AddOriginalRtpPacketForTranslation(packet);
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
            const auto playing = _rtpPacketsPlayer->IsPlaying(stream->GetOriginalSsrc());
            const auto saveInfo = !added || stream->GetAddedPacketsCount() < 2UL;
            for (const auto consumer : _consumers.ConstRef()) {
                const auto reject = added && (playing || stream->IsConnected(consumer));
                if (reject) {
                    packet->AddRejectedConsumer(consumer);
                }
                if (saveInfo || !reject) {
                    stream->SaveProducerRtpPacketInfo(consumer, packet);
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
    auto fileEndPoint = std::make_shared<FileEndPoint>(_mockTranslationFileName, GetId());
    if (!fileEndPoint->IsValid()) {
        MS_ERROR_STD("failed open %s as mock translation", _mockTranslationFileName);
    }
    else {
        fileEndPoint->SetIntervalBetweenTranslationsMs(_mockTranslationFileNameLenMs + 1000U);
        fileEndPoint->SetConnectionDelay(500U);
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

Translator::SourceStream::SourceStream(uint32_t clockRate, uint32_t originalSsrc,
                                       uint8_t payloadType,
                                       std::unique_ptr<MediaFrameSerializer> serializer,
                                       std::unique_ptr<RtpDepacketizer> depacketizer,
                                       TranslatorEndPointFactory* endPointsFactory,
                                       RtpPacketsPlayer* rtpPacketsPlayer,
                                       RtpPacketsCollector* output,
                                       const std::string& producerId)
    : _originalSsrc(originalSsrc)
    , _payloadType(payloadType)
    , _serializer(std::move(serializer))
    , _depacketizer(std::move(depacketizer))
    , _rtpPacketsPlayer(rtpPacketsPlayer)
    , _output(output)
#ifdef READ_PRODUCER_RECV_FROM_FILE
    , _fileReader(CreateFileReader())
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    , _fileWriter(CreateFileWriter(GetOriginalSsrc(), producerId, _serializer.get()))
#endif
    , _consumersManager(endPointsFactory, GetMediaSource() , GetMediaReceiver())
{
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter && !_serializer->AddTestSink(_fileWriter.get())) {
        // TODO: log error
        _fileWriter->DeleteFromStorage();
        _fileWriter.reset();
    }
#endif
    _rtpPacketsPlayer->AddStream(GetOriginalSsrc(), GetClockRate(),
                                 GetPayloadType(), GetMime(), this);
}

Translator::SourceStream::~SourceStream()
{
    GetMediaSource()->RemoveAllSinks();
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter) {
        _serializer->RemoveTestSink();
    }
#endif
    _rtpPacketsPlayer->RemoveStream(GetOriginalSsrc());
}

std::shared_ptr<Translator::SourceStream> Translator::SourceStream::Create(const RtpCodecMimeType& mime,
                                                                           uint32_t clockRate,
                                                                           uint32_t originalSsrc,
                                                                           uint8_t payloadType,
                                                                           TranslatorEndPointFactory* endPointsFactory,
                                                                           RtpPacketsPlayer* rtpPacketsPlayer,
                                                                           RtpPacketsCollector* output,
                                                                           const std::string& producerId)
{
    std::shared_ptr<SourceStream> stream;
    if (endPointsFactory && rtpPacketsPlayer && output) {
        if (auto depacketizer = RtpDepacketizer::Create(mime, clockRate)) {
            auto serializer = std::make_unique<WebMSerializer>(mime);
            stream = std::make_shared<SourceStream>(clockRate, originalSsrc, payloadType,
                                                    std::move(serializer),
                                                    std::move(depacketizer),
                                                    endPointsFactory,
                                                    rtpPacketsPlayer,
                                                    output, producerId);
        }
        else {
            MS_ERROR_STD("failed to create depacketizer for %s",
                         GetStreamInfoString(mime, originalSsrc).c_str());
        }
    }
    return stream;
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
        _consumersManager->AddConsumer(consumer);
    }
}

void Translator::SourceStream::UpdateConsumer(Consumer* consumer)
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
        _consumersManager->RemoveConsumer(consumer);
    }
}

bool Translator::SourceStream::IsConnected(Consumer* consumer) const
{
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_consumersManager);
        if (const auto info = _consumersManager->GetConsumer(consumer)) {
            return info->IsConnected();
        }
    }
    return false;
}

void Translator::SourceStream::SaveProducerRtpPacketInfo(Consumer* consumer,
                                                         const RtpPacket* packet)
{
    if (consumer && packet) {
        LOCK_READ_PROTECTED_OBJ(_consumersManager);
        if (const auto info = _consumersManager->GetConsumer(consumer)) {
            info->SaveProducerRtpPacketInfo(packet);
        }
    }
}

#ifdef READ_PRODUCER_RECV_FROM_FILE
std::unique_ptr<FileReader> Translator::SourceStream::CreateFileReader()
{
    auto fileReader = std::make_unique<FileReader>(_testFileName, false);
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

MediaSource* Translator::SourceStream::GetMediaSource() const
{
#ifdef READ_PRODUCER_RECV_FROM_FILE
    if (_fileReader) {
        return _fileReader.get();
    }
#endif
    return _serializer.get();
}

void Translator::SourceStream::OnPlayStarted(uint32_t ssrc, uint64_t mediaId,
                                             uint64_t mediaSourceId)
{
    RtpPacketsPlayerCallback::OnPlayStarted(ssrc, mediaId, mediaSourceId);
    LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
    _consumersManager->BeginPacketsSending(mediaId, mediaSourceId);
}

void Translator::SourceStream::OnPlay(const Timestamp& timestampOffset, RtpPacket* packet,
                                      uint64_t mediaId, uint64_t mediaSourceId)
{
    if (packet) {
        const auto rtpOffset = timestampOffset.GetRtpTime();
        LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
        _consumersManager->SendPacket(rtpOffset, mediaId, mediaSourceId, packet, _output);
    }
}

void Translator::SourceStream::OnPlayFinished(uint32_t ssrc, uint64_t mediaId,
                                              uint64_t mediaSourceId)
{
    RtpPacketsPlayerCallback::OnPlayFinished(ssrc, mediaId, mediaSourceId);
    LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
    _consumersManager->EndPacketsSending(mediaId, mediaSourceId);
}

void Translator::SourceStream::WriteMediaPayload(const MediaObject& sender,
                                                 const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer) {
        _rtpPacketsPlayer->Play(GetOriginalSsrc(), sender.GetId(), buffer);
    }
}

} // namespace RTC
