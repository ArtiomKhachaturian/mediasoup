#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#ifdef READ_PRODUCER_RECV_FROM_FILE
#include "RTC/MediaTranslate/FileReader.hpp"
#endif
#include "RTC/RtpStream.hpp"
#include "RTC/Producer.hpp"
#include "Logger.hpp"

namespace RTC
{

class ProducerTranslator::StreamInfo : public RtpPacketsCollector, private MediaSink
{
public:
    StreamInfo(uint32_t clockRate, uint32_t mappedSsrc,
               std::unique_ptr<MediaFrameSerializer> serializer,
               std::unique_ptr<RtpDepacketizer> depacketizer);
    ~StreamInfo();
    static std::shared_ptr<StreamInfo> Create(const RtpCodecMimeType& mime,
                                              uint32_t clockRate,
                                              uint32_t mappedSsrc,
                                              const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory);
    uint32_t GetClockRate() const { return _clockRate; }
    uint32_t GetMappedSsrc() const { return _mappedSsrc; }
    uint32_t GetOriginalSsrc() const { return _originalSsrc.load(); }
    void SetOriginalSsrc(uint32_t ssrc);
    uint8_t GetPayloadType() const { return _payloadType.load(); }
    void SetPayloadType(uint8_t payloadType);
    uint32_t GetLastOriginalRtpTimestamp() const { return _lastOriginalRtpTimestamp.load(); }
    void SetLastOriginalRtpTimestamp(uint32_t timestamp) { _lastOriginalRtpTimestamp = timestamp; }
    uint16_t GetLastOriginalRtpSeqNumber() const { return _lastOriginalRtpSeqNumber.load(); }
    void SetLastOriginalRtpSeqNumber(uint16_t seqNumber) { _lastOriginalRtpSeqNumber = seqNumber; }
    const RtpCodecMimeType& GetMime() const { return _depacketizer->GetMimeType(); }
    void SetProducerSink(MediaSink* sink);
    // impl. of RtpPacketsCollector
    bool AddPacket(RtpPacket* packet) final;
private:
    // impl. of MediaSink
    void StartMediaWriting(bool restart) final;
    void WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<const MemoryBuffer>& buffer) final;
    void EndMediaWriting() final;
#ifdef READ_PRODUCER_RECV_FROM_FILE
    static std::unique_ptr<FileReader> CreateFileReader();
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string_view& fileExtension);
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const MediaFrameSerializer* serializer,
                                                        const RtpDepacketizer* depacketizer);
#endif
private:
#ifdef READ_PRODUCER_RECV_FROM_FILE
    //static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/translation_39.webm";
    static inline const char* _testFileName = "/Users/user/Downloads/1b0cefc4-abdb-48d0-9c50-f5050755be94.webm";
#endif
    const uint32_t _clockRate;
    const uint32_t _mappedSsrc;
    const std::unique_ptr<MediaFrameSerializer> _serializer;
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
#ifdef READ_PRODUCER_RECV_FROM_FILE
    const std::unique_ptr<FileReader> _fileReader;
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    const std::unique_ptr<FileWriter> _fileWriter;
#endif
    std::atomic<uint32_t> _originalSsrc = 0U;
    std::atomic<uint8_t> _payloadType = 0U;
    std::atomic<uint32_t> _lastOriginalRtpTimestamp = 0U;
    std::atomic<uint16_t> _lastOriginalRtpSeqNumber = 0U;
    ProtectedObj<MediaSink*> _producerSink = nullptr;
};

ProducerTranslator::ProducerTranslator(Producer* producer,
                                       const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory)
    : _producer(producer)
    , _serializationFactory(serializationFactory)
{
    MS_ASSERT(_producer, "producer must not be null");
    MS_ASSERT(_serializationFactory, "media frame serialization factory must not be null");
    if (_producer->IsPaused()) {
        Pause();
    }
}

ProducerTranslator::~ProducerTranslator()
{
    for (const auto mappedSsrc : GetAddedStreams()) {
        RemoveStream(mappedSsrc);
    }
    RemoveAllSinks();
}

std::unique_ptr<ProducerTranslator> ProducerTranslator::Create(Producer* producer,
                                                               const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory)
{
    if (producer && serializationFactory) {
        return std::make_unique<ProducerTranslator>(producer, serializationFactory);
    }
    return nullptr;
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
                                             mappedSsrc,
                                             stream->GetSsrc(),
                                             stream->GetPayloadType());
}

bool ProducerTranslator::AddStream(const RtpCodecMimeType& mime, uint32_t clockRate,
                                   uint32_t mappedSsrc, uint32_t originalSsrc,
                                   uint8_t payloadType)
{
    bool ok = false, newStream = false;
    if (mappedSsrc && mime.IsMediaCodec()) {
        MS_ASSERT(mime.IsAudioCodec() == IsAudio(), "mime types mistmatch");
        MS_ASSERT(clockRate, "clock rate must be greater than zero");
        LOCK_WRITE_PROTECTED_OBJ(_streams);
        auto& streams = _streams.Ref();
        const auto it = streams.find(mappedSsrc);
        if (it == streams.end()) {
            auto streamInfo = StreamInfo::Create(mime, clockRate, mappedSsrc, _serializationFactory);
            if (streamInfo) {
                LOCK_WRITE_PROTECTED_OBJ(_originalToMappedSsrcs);
                auto& originalToMappedSsrcs = _originalToMappedSsrcs.Ref();
                originalToMappedSsrcs[originalSsrc] = mappedSsrc;
                streamInfo->SetOriginalSsrc(originalSsrc);
                streamInfo->SetPayloadType(payloadType);
                if (HasSinks()) {
                    streamInfo->SetProducerSink(this);
                }
                streams[mappedSsrc] = std::move(streamInfo);
                newStream = ok = true;
            }
            else {
                const auto desc = GetStreamInfoString(mime, mappedSsrc);
                MS_ERROR_STD("depacketizer or serializer is not available for stream [%s]", desc.c_str());
            }
        }
        else {
            LOCK_WRITE_PROTECTED_OBJ(_originalToMappedSsrcs);
            auto& originalToMappedSsrcs = _originalToMappedSsrcs.Ref();
            originalToMappedSsrcs[originalSsrc] = mappedSsrc;
            it->second->SetOriginalSsrc(originalSsrc);
            it->second->SetPayloadType(payloadType);
            ok = true; // already registered
        }
    }
    if (newStream) {
        InvokeObserverMethod(&ProducerObserver::onStreamAdded, mappedSsrc, mime, clockRate);
    }
    return ok;
}

bool ProducerTranslator::RemoveStream(uint32_t mappedSsrc)
{
    std::optional<RtpCodecMimeType> removed;
    if (mappedSsrc) {
        LOCK_WRITE_PROTECTED_OBJ(_streams);
        const auto its = _streams.Ref().find(mappedSsrc);
        if (its != _streams.Ref().end()) {
            LOCK_WRITE_PROTECTED_OBJ(_originalToMappedSsrcs);
            const auto it = _originalToMappedSsrcs.Ref().find(its->second->GetOriginalSsrc());
            if (it != _originalToMappedSsrcs.Ref().end()) {
                _originalToMappedSsrcs.Ref().erase(it);
            }
            removed = its->second->GetMime();
            _streams.Ref().erase(its);
        }
    }
    if (removed.has_value()) {
        InvokeObserverMethod(&ProducerObserver::onStreamRemoved, mappedSsrc, removed.value());
        return true;
    }
    return false;
}

std::list<uint32_t> ProducerTranslator::GetAddedStreams() const
{
    std::list<uint32_t> ssrcs;
    {
        LOCK_READ_PROTECTED_OBJ(_streams);
        const auto& streams = _streams.ConstRef();
        for (auto it = streams.begin(); it != streams.end(); ++it) {
            ssrcs.push_back(it->first);
        }
    }
    return ssrcs;
}

uint8_t ProducerTranslator::GetPayloadType(uint32_t ssrc) const
{
    if (const auto stream = GetStream(ssrc)) {
        return stream->GetPayloadType();
    }
    return 0U;
}

uint16_t ProducerTranslator::GetLastOriginalRtpSeqNumber(uint32_t ssrc) const
{
    if (const auto stream = GetStream(ssrc)) {
        return stream->GetLastOriginalRtpSeqNumber();
    }
    return 0U;
}

uint32_t ProducerTranslator::GetLastOriginalRtpTimestamp(uint32_t ssrc) const
{
    if (const auto stream = GetStream(ssrc)) {
        return stream->GetLastOriginalRtpTimestamp();
    }
    return 0U;
}

uint32_t ProducerTranslator::GetClockRate(uint32_t ssrc) const
{
    if (const auto stream = GetStream(ssrc)) {
        return stream->GetClockRate();
    }
    return 0U;
}

const std::string& ProducerTranslator::GetId() const
{
    return _producer->id;
}

bool ProducerTranslator::AddPacket(RtpPacket* packet)
{
    if (packet && !IsPaused()) {
        if (const auto stream = GetStream(packet->GetSsrc())) {
            return stream->AddPacket(packet);
        }
    }
    return false;
}

std::optional<FBS::TranslationPack::Language> ProducerTranslator::GetLanguage() const
{
    return _producer->GetLanguage();
}

std::shared_ptr<ProducerTranslator::StreamInfo> ProducerTranslator::GetStream(uint32_t ssrc) const
{
    if (ssrc) {
        LOCK_READ_PROTECTED_OBJ(_streams);
        auto its = _streams.ConstRef().find(ssrc);
        if (its == _streams.ConstRef().end()) {
            LOCK_READ_PROTECTED_OBJ(_originalToMappedSsrcs);
            const auto itm = _originalToMappedSsrcs.ConstRef().find(ssrc);
            if (itm != _originalToMappedSsrcs.ConstRef().end()) {
                its = _streams.ConstRef().find(itm->second);
            }
        }
        if (its != _streams.ConstRef().end()) {
            return its->second;
        }
    }
    return nullptr;
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

bool ProducerTranslator::IsSinkValid(const MediaSink* sink) const
{
    return sink != this;
}

void ProducerTranslator::OnFirstSinkAdded()
{
    MediaSource::OnFirstSinkAdded();
    LOCK_READ_PROTECTED_OBJ(_streams);
    const auto& streams = _streams.ConstRef();
    for (auto it = streams.begin(); it != streams.end(); ++it) {
        it->second->SetProducerSink(this);
    }
}

void ProducerTranslator::OnLastSinkRemoved()
{
    MediaSource::OnLastSinkRemoved();
    LOCK_READ_PROTECTED_OBJ(_streams);
    const auto& streams = _streams.ConstRef();
    for (auto it = streams.begin(); it != streams.end(); ++it) {
        it->second->SetProducerSink(nullptr);
    }
}

void ProducerTranslator::StartMediaWriting(bool restart)
{
    StartMediaSinksWriting(restart);
}

void ProducerTranslator::WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<const MemoryBuffer>& buffer)
{
    WriteMediaSinksPayload(ssrc, buffer);
}

void ProducerTranslator::EndMediaWriting()
{
    EndMediaSinksWriting();
}

ProducerTranslator::StreamInfo::StreamInfo(uint32_t clockRate, uint32_t mappedSsrc,
                                           std::unique_ptr<MediaFrameSerializer> serializer,
                                           std::unique_ptr<RtpDepacketizer> depacketizer)
    : _clockRate(clockRate)
    , _mappedSsrc(mappedSsrc)
    , _serializer(std::move(serializer))
    , _depacketizer(std::move(depacketizer))
#ifdef READ_PRODUCER_RECV_FROM_FILE
    , _fileReader(CreateFileReader())
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    , _fileWriter(CreateFileWriter(mappedSsrc, _serializer.get(), _depacketizer.get()))
#endif
{
}

ProducerTranslator::StreamInfo::~StreamInfo()
{
    SetProducerSink(nullptr);
}

std::shared_ptr<ProducerTranslator::StreamInfo> ProducerTranslator::StreamInfo::Create(const RtpCodecMimeType& mime,
                                                                                       uint32_t clockRate,
                                                                                       uint32_t mappedSsrc,
                                                                                       const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory)
{
    if (serializationFactory) {
        if (auto serializer = serializationFactory->CreateSerializer()) {
            bool configured = false;
            switch (mime.GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    configured = serializer->AddAudio(mappedSsrc, clockRate, mime.GetSubtype());
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    configured = serializer->AddVideo(mappedSsrc, clockRate, mime.GetSubtype());
                    break;
            }
            if (configured) {
                if (auto depacketizer = RtpDepacketizer::create(mime, clockRate)) {
                    return std::make_shared<StreamInfo>(clockRate, mappedSsrc,
                                                        std::move(serializer),
                                                        std::move(depacketizer));
                }
            }
        }
    }
    return nullptr;
}

void ProducerTranslator::StreamInfo::SetOriginalSsrc(uint32_t ssrc)
{
    if (ssrc != _originalSsrc.exchange(ssrc)) {
#ifdef READ_PRODUCER_RECV_FROM_FILE
        if (_fileReader) {
            _fileReader->SetSsrc(ssrc);
        }
#endif
    }
}

void ProducerTranslator::StreamInfo::SetPayloadType(uint8_t payloadType)
{
    const auto oldPayloadType = _payloadType.exchange(payloadType);
    if (oldPayloadType && oldPayloadType != payloadType) {
        MS_WARN_DEV_STD("payload type changed from %d to %d for stream %s",
                        int(oldPayloadType), int(payloadType),
                        GetStreamInfoString(GetMime(), GetMappedSsrc()).c_str());
    }
}

void ProducerTranslator::StreamInfo::SetProducerSink(MediaSink* sink)
{
    MS_ASSERT(this != sink, "wrong reference to itself sink");
    LOCK_WRITE_PROTECTED_OBJ(_producerSink);
    if (sink != _producerSink.ConstRef()) {
        MediaSource* source = _serializer.get();
#ifdef READ_PRODUCER_RECV_FROM_FILE
        if (_fileReader) {
            source = _fileReader.get();
        }
#endif
        _producerSink = sink;
        if (sink) {
            source->AddSink(this);
        }
        else {
            source->RemoveSink(this);
        }
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter) {
        if (sink) {
            source->AddSink(_fileWriter.get());
        }
        else {
            source->RemoveSink(_fileWriter.get());
        }
    }
#endif
    }
}

bool ProducerTranslator::StreamInfo::AddPacket(RtpPacket* packet)
{
    if (packet) {
        SetLastOriginalRtpTimestamp(packet->GetTimestamp());
        SetLastOriginalRtpSeqNumber(packet->GetSequenceNumber());
#ifdef READ_PRODUCER_RECV_FROM_FILE
        if (_fileReader) {
            return true;
        }
#endif
        if (_depacketizer) {
            if (const auto frame = _depacketizer->AddPacket(packet)) {
                return _serializer->Push(GetMappedSsrc(), frame);
            }
        }
    }
    return false;
}

void ProducerTranslator::StreamInfo::StartMediaWriting(bool restart)
{
    MediaSink::StartMediaWriting(restart);
    LOCK_READ_PROTECTED_OBJ(_producerSink);
    if (const auto sink = _producerSink.ConstRef()) {
        sink->StartMediaWriting(restart);
    }
}

void ProducerTranslator::StreamInfo::WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<const MemoryBuffer>& buffer)
{
    LOCK_READ_PROTECTED_OBJ(_producerSink);
    if (const auto sink = _producerSink.ConstRef()) {
        if (ssrc == GetMappedSsrc()) {
            ssrc = GetOriginalSsrc();
        }
        sink->WriteMediaPayload(ssrc, buffer);
    }
}

void ProducerTranslator::StreamInfo::EndMediaWriting()
{
    MediaSink::EndMediaWriting();
    LOCK_READ_PROTECTED_OBJ(_producerSink);
    if (const auto sink = _producerSink.ConstRef()) {
        sink->EndMediaWriting();
    }
}

#ifdef READ_PRODUCER_RECV_FROM_FILE
std::unique_ptr<FileReader> ProducerTranslator::StreamInfo::CreateFileReader()
{
    auto fileReader = std::make_unique<FileReader>(_testFileName, true);
    if (fileReader->IsOpen()) {
        return fileReader;
    }
    return nullptr;
}
#endif

#ifdef WRITE_PRODUCER_RECV_TO_FILE
std::unique_ptr<FileWriter> ProducerTranslator::StreamInfo::CreateFileWriter(uint32_t ssrc,
                                                                             const std::string_view& fileExtension)
{
    if (!fileExtension.empty()) {
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = std::to_string(ssrc) + "." + std::string(fileExtension);
            fileName = std::string(depacketizerPath) + "/" + fileName;
            auto fileWriter = std::make_unique<FileWriter>(fileName);
            if (fileWriter->IsOpen()) {
                return fileWriter;
            }
        }
    }
    return nullptr;
}

std::unique_ptr<FileWriter> ProducerTranslator::StreamInfo::CreateFileWriter(uint32_t ssrc,
                                                                             const MediaFrameSerializer* serializer,
                                                                             const RtpDepacketizer* depacketizer)
{
    if (serializer && depacketizer) {
        return CreateFileWriter(ssrc, serializer->GetFileExtension(depacketizer->GetMimeType()));
    }
    return nullptr;
}
#endif

} // namespace RTC
