#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#ifdef READ_PRODUCER_RECV_FROM_FILE
#include "RTC/MediaTranslate/FileReader.hpp"
#endif
#include "RTC/RtpStream.hpp"
#include "RTC/Producer.hpp"
#include "Logger.hpp"

namespace {

class SsrcProvider
{
public:
    virtual uint32_t GetMappedSsrc() const = 0;
    virtual uint32_t GetOriginalSsrc() const = 0;
protected:
    virtual ~SsrcProvider() = default;
};

class MediaSinkWrapper : public RTC::MediaSink
{
public:
    MediaSinkWrapper(RTC::MediaSink* impl, const SsrcProvider* ssrcProvider);
    void StartMediaWriting(uint32_t ssrc) final;
    void WriteMediaPayload(const std::shared_ptr<RTC::MemoryBuffer>& buffer) final;
    void EndMediaWriting() final;
private:
    RTC::MediaSink* const _impl;
    const SsrcProvider* const _ssrcProvider;
};

}

namespace RTC
{

class ProducerTranslator::StreamInfo : public RtpPacketsCollector,
                                       public SsrcProvider
{
public:
    StreamInfo(uint32_t clockRate, uint32_t mappedSsrc,
               const std::string& producerId,
               std::unique_ptr<MediaFrameSerializer> serializer,
               std::unique_ptr<RtpDepacketizer> depacketizer);
    ~StreamInfo() final;
    static std::shared_ptr<StreamInfo> Create(const RtpCodecMimeType& mime,
                                              uint32_t clockRate, uint32_t mappedSsrc,
                                              const std::string& producerId,
                                              const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory);
    uint32_t GetClockRate() const { return _serializer->GetClockRate(); }
    void SetOriginalSsrc(uint32_t ssrc);
    uint8_t GetPayloadType() const { return _payloadType.load(); }
    void SetPayloadType(uint8_t payloadType);
    uint32_t GetLastOriginalRtpTimestamp() const { return _lastOriginalRtpTimestamp.load(); }
    void SetLastOriginalRtpTimestamp(uint32_t timestamp);
    uint16_t GetLastOriginalRtpSeqNumber() const { return _lastOriginalRtpSeqNumber.load(); }
    void SetLastOriginalRtpSeqNumber(uint16_t seqNumber);
    const RtpCodecMimeType& GetMime() const { return _depacketizer->GetMimeType(); }
    // impl. of MediaSource
    void AddSink(MediaSink* sink);
    void RemoveSink(MediaSink* sink);
    void RemoveAllSinks();
    // impl. of RtpPacketsCollector
    bool AddPacket(RtpPacket* packet) final;
    // impl. of SsrcProvider
    uint32_t GetMappedSsrc() const final { return _serializer->GetSsrc(); }
    uint32_t GetOriginalSsrc() const final { return _originalSsrc.load(); }
private:
    MediaSource* GetMediaSource() const;
#ifdef READ_PRODUCER_RECV_FROM_FILE
    static std::unique_ptr<FileReader> CreateFileReader();
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
    static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/producer_test.webm";
#endif
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
    ProtectedObj<absl::flat_hash_map<MediaSink*, std::unique_ptr<MediaSinkWrapper>>> _sinkWrappers;
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
            auto streamInfo = StreamInfo::Create(mime, clockRate, mappedSsrc, GetId(), _serializationFactory);
            if (streamInfo) {
                {
                    LOCK_READ_PROTECTED_OBJ(_sinks);
                    for (auto its = _sinks.ConstRef().begin(); its != _sinks.ConstRef().end(); ++its) {
                        streamInfo->AddSink(*its);
                    }
                }
                LOCK_WRITE_PROTECTED_OBJ(_originalToMappedSsrcs);
                auto& originalToMappedSsrcs = _originalToMappedSsrcs.Ref();
                originalToMappedSsrcs[originalSsrc] = mappedSsrc;
                streamInfo->SetOriginalSsrc(originalSsrc);
                streamInfo->SetPayloadType(payloadType);
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

const std::string& ProducerTranslator::GetLanguageId() const
{
    return _producer->GetLanguageId();
}

bool ProducerTranslator::AddSink(MediaSink* sink)
{
    if (sink) {
        LOCK_WRITE_PROTECTED_OBJ(_sinks);
        auto& sinks = _sinks.Ref();
        if (sinks.end() == std::find(sinks.begin(), sinks.end(), sink)) {
            sinks.push_back(sink);
            LOCK_READ_PROTECTED_OBJ(_streams);
            for (auto it = _streams.ConstRef().begin(); it != _streams.ConstRef().end(); ++it) {
                it->second->AddSink(sink);
            }
            return true;
        }
    }
    return false;
}

bool ProducerTranslator::RemoveSink(MediaSink* sink)
{
    if (sink) {
        LOCK_WRITE_PROTECTED_OBJ(_sinks);
        auto& sinks = _sinks.Ref();
        const auto it = std::find(sinks.begin(), sinks.end(), sink);
        if (it != sinks.end()) {
            sinks.erase(it);
            LOCK_READ_PROTECTED_OBJ(_streams);
            for (auto it = _streams.ConstRef().begin(); it != _streams.ConstRef().end(); ++it) {
                it->second->RemoveSink(sink);
            }
            return true;
        }
    }
    return false;
}

void ProducerTranslator::RemoveAllSinks()
{
    LOCK_WRITE_PROTECTED_OBJ(_sinks);
    LOCK_READ_PROTECTED_OBJ(_streams);
    for (auto it = _streams.ConstRef().begin(); it != _streams.ConstRef().end(); ++it) {
        it->second->RemoveAllSinks();
    }
    _sinks.Ref().clear();
}

bool ProducerTranslator::HasSinks() const
{
    LOCK_READ_PROTECTED_OBJ(_sinks);
    return !_sinks.ConstRef().empty();
}

size_t ProducerTranslator::GetSinksCout() const
{
    LOCK_READ_PROTECTED_OBJ(_sinks);
    return _sinks.ConstRef().size();
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

ProducerTranslator::StreamInfo::StreamInfo(uint32_t clockRate, uint32_t mappedSsrc,
                                           const std::string& producerId,
                                           std::unique_ptr<MediaFrameSerializer> serializer,
                                           std::unique_ptr<RtpDepacketizer> depacketizer)
    : _serializer(std::move(serializer))
    , _depacketizer(std::move(depacketizer))
#ifdef READ_PRODUCER_RECV_FROM_FILE
    , _fileReader(CreateFileReader())
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    , _fileWriter(CreateFileWriter(mappedSsrc, producerId, _serializer.get()))
#endif
{
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter) {
        if (const auto source = GetMediaSource()) {
            source->AddSink(_fileWriter.get());
        }
    }
#endif
}

ProducerTranslator::StreamInfo::~StreamInfo()
{
    RemoveAllSinks();
}

std::shared_ptr<ProducerTranslator::StreamInfo> ProducerTranslator::StreamInfo::Create(const RtpCodecMimeType& mime,
                                                                                       uint32_t clockRate,
                                                                                       uint32_t mappedSsrc,
                                                                                       const std::string& producerId,
                                                                                       const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory)
{
    if (serializationFactory) {
        if (auto serializer = serializationFactory->CreateSerializer(mappedSsrc, clockRate, mime)) {
            if (auto depacketizer = RtpDepacketizer::create(mime, clockRate)) {
                return std::make_shared<StreamInfo>(clockRate, mappedSsrc, producerId,
                                                    std::move(serializer), std::move(depacketizer));
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

void ProducerTranslator::StreamInfo::SetLastOriginalRtpTimestamp(uint32_t timestamp)
{
    _lastOriginalRtpTimestamp = timestamp;
}

void ProducerTranslator::StreamInfo::SetLastOriginalRtpSeqNumber(uint16_t seqNumber)
{
    _lastOriginalRtpSeqNumber = seqNumber;
}

void ProducerTranslator::StreamInfo::AddSink(MediaSink* sink)
{
    if (sink) {
        if (const auto source = GetMediaSource()) {
            LOCK_WRITE_PROTECTED_OBJ(_sinkWrappers);
            auto& sinkWrappers = _sinkWrappers.Ref();
            if (sinkWrappers.end() == sinkWrappers.find(sink)) {
                auto sinkWrapper = std::make_unique<MediaSinkWrapper>(sink, this);
                if (source->AddSink(sinkWrapper.get())) {
                    sinkWrappers[sink] = std::move(sinkWrapper);
                }
            }
        }
    }
}

void ProducerTranslator::StreamInfo::RemoveSink(MediaSink* sink)
{
    if (sink) {
        if (const auto source = GetMediaSource()) {
            LOCK_WRITE_PROTECTED_OBJ(_sinkWrappers);
            auto& sinkWrappers = _sinkWrappers.Ref();
            const auto it = sinkWrappers.find(sink);
            if (it != sinkWrappers.end() && source->RemoveSink(it->second.get())) {
                sinkWrappers.erase(it);
            }
        }
    }
}

void ProducerTranslator::StreamInfo::RemoveAllSinks()
{
    if (const auto source = GetMediaSource()) {
        LOCK_WRITE_PROTECTED_OBJ(_sinkWrappers);
        auto& sinkWrappers = _sinkWrappers.Ref();
        for (auto it = sinkWrappers.begin(); it != sinkWrappers.end(); ++it) {
            source->RemoveSink(it->second.get());
        }
        sinkWrappers.clear();
#ifdef WRITE_PRODUCER_RECV_TO_FILE
        if (_fileWriter) {
            source->RemoveSink(_fileWriter.get());
        }
#endif
    }
}

bool ProducerTranslator::StreamInfo::AddPacket(RtpPacket* packet)
{
    if (packet) {
        SetLastOriginalRtpTimestamp(packet->GetTimestamp());
        SetLastOriginalRtpSeqNumber(packet->GetSequenceNumber());
        if (!packet->IsSynthenized()) {
#ifdef READ_PRODUCER_RECV_FROM_FILE
            if (_fileReader) {
                return true;
            }
#endif
            if (const auto frame = _depacketizer->AddPacket(packet)) {
                return _serializer->Push(frame);
            }
        }
    }
    return false;
}

MediaSource* ProducerTranslator::StreamInfo::GetMediaSource() const
{
    MediaSource* source = _serializer.get();
#ifdef READ_PRODUCER_RECV_FROM_FILE
    if (_fileReader) {
        source = _fileReader.get();
    }
#endif
    return source;
}

#ifdef READ_PRODUCER_RECV_FROM_FILE
std::unique_ptr<FileReader> ProducerTranslator::StreamInfo::CreateFileReader()
{
    auto fileReader = std::make_unique<FileReader>(_testFileName, false);
    if (fileReader->IsOpen()) {
        return fileReader;
    }
    MS_WARN_DEV_STD("Failed to open producer file input %s", _testFileName);
    return nullptr;
}
#endif

#ifdef WRITE_PRODUCER_RECV_TO_FILE
std::unique_ptr<FileWriter> ProducerTranslator::StreamInfo::CreateFileWriter(uint32_t ssrc,
                                                                             const std::string& producerId,
                                                                             const std::string_view& fileExtension)
{
    if (!fileExtension.empty()) {
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = producerId + "_" + std::to_string(ssrc) + "." + std::string(fileExtension);
            fileName = std::string(depacketizerPath) + "/" + fileName;
            auto fileWriter = std::make_unique<FileWriter>(fileName);
            if (fileWriter->IsOpen()) {
                return fileWriter;
            }
            MS_WARN_DEV_STD("Failed to open producer file output %s", fileName.c_str());
        }
    }
    return nullptr;
}

std::unique_ptr<FileWriter> ProducerTranslator::StreamInfo::CreateFileWriter(uint32_t ssrc,
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

namespace {

MediaSinkWrapper::MediaSinkWrapper(RTC::MediaSink* impl, const SsrcProvider* ssrcProvider)
    : _impl(impl)
    , _ssrcProvider(ssrcProvider)
{
}

void MediaSinkWrapper::StartMediaWriting(uint32_t ssrc)
{
    RTC::MediaSink::StartMediaWriting(ssrc);
    if (ssrc == _ssrcProvider->GetMappedSsrc()) {
        ssrc = _ssrcProvider->GetOriginalSsrc();
    }
    _impl->StartMediaWriting(ssrc);
}

void MediaSinkWrapper::WriteMediaPayload(const std::shared_ptr<RTC::MemoryBuffer>& buffer)
{
    if (buffer) {
        _impl->WriteMediaPayload(buffer);
    }
}

void MediaSinkWrapper::EndMediaWriting()
{
    RTC::MediaSink::EndMediaWriting();
    _impl->EndMediaWriting();
}

}
