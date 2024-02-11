#define MS_CLASS "RTC::ProducerTranslator"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#ifdef NO_TRANSLATION_SERVICE
#include "RTC/MediaTranslate/TranslationEndPoint/MockEndPoint.hpp"
#else
#include "RTC/MediaTranslate/TranslationEndPoint/WebsocketEndPoint.hpp"
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

#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
namespace {

std::atomic<uint64_t> g_InstancesCounter = 0ULL;

}
#endif

namespace RTC
{

class ProducerTranslator::StreamInfo
{
public:
    StreamInfo(uint32_t clockRate, uint8_t payloadType, const std::string& producerId,
               std::unique_ptr<MediaFrameSerializer> serializer,
               std::unique_ptr<RtpDepacketizer> depacketizer);
    ~StreamInfo();
    static std::shared_ptr<StreamInfo> Create(const RtpCodecMimeType& mime,
                                              uint32_t clockRate,
                                              uint32_t originalSsrc,
                                              uint8_t payloadType,
                                              const std::string& producerId);
    uint32_t GetClockRate() const { return _serializer->GetClockRate(); }
    uint8_t GetPayloadType() const { return _payloadType; }
    uint32_t GetLastOriginalRtpTimestamp() const { return _lastOriginalRtpTimestamp.load(); }
    void SetLastOriginalRtpTimestamp(uint32_t timestamp);
    uint16_t GetLastOriginalRtpSeqNumber() const { return _lastOriginalRtpSeqNumber.load(); }
    void SetLastOriginalRtpSeqNumber(uint16_t seqNumber);
    const RtpCodecMimeType& GetMime() const { return _depacketizer->GetMimeType(); }
    uint32_t GetOriginalSsrc() const { return _serializer->GetSsrc(); }
    void AddSink(MediaSink* sink);
    void RemoveSink(MediaSink* sink);
    void RemoveAllSinks();
    bool AddOriginalRtpPacketForTranslation(RtpPacket* packet);
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
    std::atomic<uint32_t> _lastOriginalRtpTimestamp = 0U;
    std::atomic<uint16_t> _lastOriginalRtpSeqNumber = 0U;
};

ProducerTranslator::ProducerTranslator(const Producer* producer,
                                       RtpPacketsPlayer* translationsOutput,
                                       const std::string& serviceUri,
                                       const std::string& serviceUser,
                                       const std::string& servicePassword)
    : _producer(producer)
    , _translationsOutput(translationsOutput)
    , _serviceUri(serviceUri)
    , _serviceUser(serviceUser)
    , _servicePassword(servicePassword)
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    , _instanceIndex(g_InstancesCounter.fetch_add(1U))
#endif
{
}

ProducerTranslator::~ProducerTranslator()
{
    {
        LOCK_WRITE_PROTECTED_OBJ(_endPoints);
#ifdef NO_TRANSLATION_SERVICE
        if (!_endPoints->empty()) {
            RemoveSink(_translationsOutput);
        }
#else
        for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
            it->second->SetInput(nullptr);
            it->second->SetOutput(nullptr);
        }
#endif
        _endPoints->clear();
    }
    for (const auto mappedSsrc : GetSsrcs(true)) {
        RemoveStream(mappedSsrc);
    }
    RemoveAllSinks();
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    g_InstancesCounter.fetch_sub(1U);
#endif
}

std::unique_ptr<ProducerTranslator> ProducerTranslator::Create(const Producer* producer,
                                                               RtpPacketsPlayer* translationsOutput,
                                                               const std::string& serviceUri,
                                                               const std::string& serviceUser,
                                                               const std::string& servicePassword)
{
    if (producer && translationsOutput && Media::Kind::AUDIO == producer->GetKind()) {
        auto translator = new ProducerTranslator(producer, translationsOutput,
                                                 serviceUri, serviceUser, servicePassword);
        // add streams
        const auto& streams = producer->GetRtpStreams();
        for (auto it = streams.begin(); it != streams.end(); ++it) {
            translator->AddStream(it->second, it->first);
        }
        return std::unique_ptr<ProducerTranslator>(translator);
    }
    return nullptr;
}

bool ProducerTranslator::AddStream(uint32_t mappedSsrc, const RtpStream* stream)
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
                auto streamInfo = StreamInfo::Create(mime, clockRate, originalSsrc,
                                                     payloadType, _producer->id);
                if (streamInfo) {
                    AddSinksToStream(streamInfo);
                    //_translationsOutput->AddStream(originalSsrc, mime, this,  this);
                    _originalSsrcToStreams[originalSsrc] = streamInfo;
                    _mappedSsrcToStreams->insert({mappedSsrc, std::move(streamInfo)});
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

bool ProducerTranslator::RemoveStream(uint32_t mappedSsrc)
{
    if (mappedSsrc) {
        LOCK_WRITE_PROTECTED_OBJ(_mappedSsrcToStreams);
        const auto it = _mappedSsrcToStreams->find(mappedSsrc);
        if (it != _mappedSsrcToStreams->end()) {
            _translationsOutput->RemoveStream(it->second->GetOriginalSsrc());
            _originalSsrcToStreams.erase(it->second->GetOriginalSsrc());
            _mappedSsrcToStreams->erase(it);
            return true;
        }
    }
    return false;
}

void ProducerTranslator::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    if (packet && !_producer->IsPaused()) {
        if (const auto stream = GetStream(packet->GetSsrc())) {
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
            const auto added = _instanceIndex > 0U || stream->AddOriginalRtpPacketForTranslation(packet);
#else
            const auto added = stream->AddOriginalRtpPacketForTranslation(packet);
#endif
            if (added) {
                LOCK_READ_PROTECTED_OBJ(_endPoints);
                for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
#ifdef NO_TRANSLATION_SERVICE
                    packet->AddRejectedConsumer(it->first);
#else
                    if (it->second->IsConnected()) {
                        packet->AddRejectedConsumer(it->first);
                    }
#endif
                }
            }
        }
    }
}

const std::string& ProducerTranslator::GetId() const
{
    return _producer->id;
}

std::list<uint32_t> ProducerTranslator::GetSsrcs(bool mapped) const
{
    std::list<uint32_t> ssrcs;
    LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
    for (auto it = _mappedSsrcToStreams->begin(); it != _mappedSsrcToStreams->end(); ++it) {
        ssrcs.push_back(mapped ? it->first : it->second->GetOriginalSsrc());
    }
    return ssrcs;
}

void ProducerTranslator::AddConsumer(Consumer* consumer)
{
    if (consumer) {
        MS_ASSERT(consumer->producerId == GetId(), "wrong producer ID");
        LOCK_WRITE_PROTECTED_OBJ(_endPoints);
        if (!_endPoints->count(consumer)) {
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
            if (_instanceIndex || !_endPoints->empty()) {
                return;
            }
#endif
            // TODO: more compliant logic required, but not for demo
            const auto endPointId = reinterpret_cast<uint64_t>(consumer);
            std::unique_ptr<TranslatorEndPoint> endPoint;
#ifdef NO_TRANSLATION_SERVICE
            endPoint = std::make_unique<MockEndPoint>(endPointId,
                                                      _mockTranslationFileName,
                                                      _mockTranslationFileNameLenMs + 1000U);
#else
            endPoint = std::make_unique<WebsocketEndPoint>(endPointId,
                                                           _serviceUri,
                                                           _serviceUser,
                                                           _servicePassword);
#endif
            TranslatorEndPoint* ref = endPoint.get();
            endPoint->SetInputLanguageId(_producer->GetLanguageId());
            endPoint->SetOutputLanguageId(consumer->GetLanguageId());
            endPoint->SetOutputVoiceId(consumer->GetVoiceId());
            _endPoints->insert({consumer, std::move(endPoint)});
            ref->SetInput(this);
            ref->SetOutput(this);
        }
    }
}

void ProducerTranslator::RemoveConsumer(Consumer* consumer)
{
    if (consumer) {
        MS_ASSERT(consumer->producerId == GetId(), "wrong producer ID");
        LOCK_WRITE_PROTECTED_OBJ(_endPoints);
        const auto it = _endPoints->find(consumer);
        if (it != _endPoints->end()) {
            it->second->SetInput(nullptr);
            it->second->SetOutput(nullptr);
            _endPoints->erase(it);
        }
    }
}

void ProducerTranslator::UpdateProducerLanguage()
{
    LOCK_READ_PROTECTED_OBJ(_endPoints);
    for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
        it->second->SetInputLanguageId(_producer->GetLanguageId());
    }
}

void ProducerTranslator::UpdateConsumerLanguage(Consumer* consumer)
{
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_endPoints);
        const auto it = _endPoints->find(consumer);
        if (it != _endPoints->end()) {
            it->second->SetOutputLanguageId(consumer->GetLanguageId());
        }
    }
}

void ProducerTranslator::UpdateConsumerVoice(Consumer* consumer)
{
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_endPoints);
        const auto it = _endPoints->find(consumer);
        if (it != _endPoints->end()) {
            it->second->SetOutputVoiceId(consumer->GetVoiceId());
        }
    }
}

std::shared_ptr<ProducerTranslator::StreamInfo> ProducerTranslator::GetStream(uint32_t ssrc) const
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

void ProducerTranslator::AddSinksToStream(const std::shared_ptr<StreamInfo>& stream) const
{
    if (stream) {
        LOCK_READ_PROTECTED_OBJ(_sinks);
        for (const auto sink : _sinks.ConstRef()) {
            stream->AddSink(sink);
        }
    }
}

bool ProducerTranslator::AddSink(MediaSink* sink)
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

bool ProducerTranslator::RemoveSink(MediaSink* sink)
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

void ProducerTranslator::RemoveAllSinks()
{
    LOCK_WRITE_PROTECTED_OBJ(_sinks);
    LOCK_READ_PROTECTED_OBJ(_mappedSsrcToStreams);
    for (auto it = _mappedSsrcToStreams->begin();
         it != _mappedSsrcToStreams->end(); ++it) {
        it->second->RemoveAllSinks();
    }
    _sinks->clear();
}

bool ProducerTranslator::HasSinks() const
{
    LOCK_READ_PROTECTED_OBJ(_sinks);
    return !_sinks->empty();
}

size_t ProducerTranslator::GetSinksCout() const
{
    LOCK_READ_PROTECTED_OBJ(_sinks);
    return _sinks.ConstRef().size();
}
    
void ProducerTranslator::OnTranslatedMediaReceived(uint64_t endPointId, uint64_t mediaSeqNum,
                                                   const std::shared_ptr<MemoryBuffer>& media)
{
    
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

ProducerTranslator::StreamInfo::StreamInfo(uint32_t clockRate, uint8_t payloadType,
                                           const std::string& producerId,
                                           std::unique_ptr<MediaFrameSerializer> serializer,
                                           std::unique_ptr<RtpDepacketizer> depacketizer)
    : _payloadType(payloadType)
    , _serializer(std::move(serializer))
    , _depacketizer(std::move(depacketizer))
#ifdef READ_PRODUCER_RECV_FROM_FILE
    , _fileReader(CreateFileReader(_serializer->GetSsrc()))
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    , _fileWriter(CreateFileWriter(_serializer->GetSsrc(), producerId, _serializer.get()))
#endif
{
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter && !_serializer->AddTestSink(_fileWriter.get())) {
        // TODO: log error
        _fileWriter->DeleteFromStorage();
        _fileWriter.reset();
    }
#endif
}

ProducerTranslator::StreamInfo::~StreamInfo()
{
    RemoveAllSinks();
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter) {
        _serializer->RemoveTestSink();
    }
#endif
}

std::shared_ptr<ProducerTranslator::StreamInfo> ProducerTranslator::StreamInfo::Create(const RtpCodecMimeType& mime,
                                                                                       uint32_t clockRate,
                                                                                       uint32_t originalSsrc,
                                                                                       uint8_t payloadType,
                                                                                       const std::string& producerId)
{
    if (WebMCodecs::IsSupported(mime)) {
        if (auto depacketizer = RtpDepacketizer::create(mime, clockRate)) {
            auto serializer = std::make_unique<WebMSerializer>(originalSsrc, clockRate, mime);
            return std::make_shared<StreamInfo>(clockRate, payloadType, producerId,
                                                std::move(serializer), std::move(depacketizer));
        }
    }
    return nullptr;
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
        MediaSource* source = _serializer.get();
#ifdef READ_PRODUCER_RECV_FROM_FILE
        if (_fileReader) {
            source = _fileReader.get();
        }
#endif
        source->AddSink(sink);
    }
}

void ProducerTranslator::StreamInfo::RemoveSink(MediaSink* sink)
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

void ProducerTranslator::StreamInfo::RemoveAllSinks()
{
    MediaSource* source = _serializer.get();
#ifdef READ_PRODUCER_RECV_FROM_FILE
    if (_fileReader) {
        source = _fileReader.get();
    }
#endif
    source->RemoveAllSinks();
}

bool ProducerTranslator::StreamInfo::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    bool handled = false;
    if (packet) {
        SetLastOriginalRtpTimestamp(packet->GetTimestamp());
        SetLastOriginalRtpSeqNumber(packet->GetSequenceNumber());
#ifdef READ_PRODUCER_RECV_FROM_FILE
        if (_fileReader) {
            return true;
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
    return handled;
}

#ifdef READ_PRODUCER_RECV_FROM_FILE
std::unique_ptr<FileReader> ProducerTranslator::StreamInfo::CreateFileReader(uint32_t ssrc)
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
            if (!fileWriter->IsOpen()) {
                fileWriter.reset();
                MS_WARN_DEV_STD("Failed to open producer file output %s", fileName.c_str());
            }
            return fileWriter;
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
