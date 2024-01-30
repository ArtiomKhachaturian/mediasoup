#define MS_CLASS "RTC::ConsumerTranslator"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpMemoryBufferPacket.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"

namespace {

struct CodecInfo
{
    const std::unique_ptr<RTC::RtpPacketizer> _packetizer;
    const uint8_t _payloadType;
    const size_t _trackIndex;
    const uint32_t _ssrc;
    uint16_t _sequenceNumber;
    RTC::MediaFrameDeserializeResult _lastError = RTC::MediaFrameDeserializeResult::Success;
    CodecInfo(std::unique_ptr<RTC::RtpPacketizer> packetizer, uint8_t payloadType,
              size_t trackIndex, uint32_t ssrc, uint16_t sequenceNumber);
};

}

namespace RTC
{

class ConsumerTranslator::MediaGrabber
{
public:
    MediaGrabber(Media::Kind kind,
                 std::unique_ptr<MediaFrameDeserializer> deserializer,
                 RtpPacketsCollector* packetsCollector,
                 const RtpParameters& rtpParameters);
    void ProcessMediaPayload(uint32_t ssrc,
                             const std::shared_ptr<const MemoryBuffer>& payload,
                             const ProducerPacketsInfo& producerPacketsInfo);
private:
    bool FetchMediaInfo(const ProducerPacketsInfo& producerPacketsInfo);
    void DeserializeMediaFrames(uint32_t ssrc);
    const RtpCodecParameters* GetCodec(const RtpCodecMimeType& mime) const;
    static const char* ToString(MediaFrameDeserializeResult result);
private:
    const bool _audio;
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    RtpPacketsCollector* const _packetsCollector;
    const RtpParameters& _rtpParameters;
    absl::flat_hash_map<RtpCodecMimeType::Subtype, std::unique_ptr<CodecInfo>> _codecs;
    MediaFrameDeserializeResult _lastProcessingError = MediaFrameDeserializeResult::Success;
};

ConsumerTranslator::ConsumerTranslator(const Consumer* consumer,
                                       RtpPacketsCollector* packetsCollector,
                                       const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory)
    : _consumer(consumer)
    , _packetsCollector(packetsCollector)
    , _serializationFactory(serializationFactory)
{
    MS_ASSERT(_consumer, "consumer must not be null");
    MS_ASSERT(_packetsCollector, "RTP packets collector must not be null");
    MS_ASSERT(_serializationFactory, "media frames serialization factory must not be null");
    if (consumer->IsPaused()) {
        Pause();
    }
}

ConsumerTranslator::~ConsumerTranslator()
{
    LOCK_WRITE_PROTECTED_OBJ(_mediaGrabber);
    _mediaGrabber->reset();
}

const std::string& ConsumerTranslator::GetId() const
{
    return _consumer->id;
}

void ConsumerTranslator::AddObserver(ConsumerObserver* observer)
{
    _observers.Add(observer);
}

void ConsumerTranslator::RemoveObserver(ConsumerObserver* observer)
{
    _observers.Remove(observer);
}

bool ConsumerTranslator::HadIncomingMedia() const
{
    LOCK_READ_PROTECTED_OBJ(_mediaGrabber);
    return nullptr != _mediaGrabber.ConstRef();
}

void ConsumerTranslator::ProcessProducerRtpPacket(const RtpPacket* packet)
{
    if (packet) {
        if (const auto payloadType = packet->GetPayloadType()) {
            LOCK_WRITE_PROTECTED_OBJ(_producerPacketsInfo);
            auto& producerPacketsInfo = _producerPacketsInfo.Ref();
            auto packetInfo = std::make_tuple(packet->GetTimestamp(), packet->GetSsrc(), packet->GetSequenceNumber());
            producerPacketsInfo[payloadType] = std::move(packetInfo);
        }
    }
}

std::optional<FBS::TranslationPack::Language> ConsumerTranslator::GetLanguage() const
{
    return _consumer->GetLanguage();
}

std::optional<FBS::TranslationPack::Voice> ConsumerTranslator::GetVoice() const
{
    return _consumer->GetVoice();
}

void ConsumerTranslator::StartMediaWriting(bool restart)
{
    MediaSink::StartMediaWriting(restart);
    if (auto deserializer = _serializationFactory->CreateDeserializer()) {
        {
            LOCK_WRITE_PROTECTED_OBJ(_mediaGrabber);
            _mediaGrabber = std::make_unique<MediaGrabber>(_consumer->GetKind(),
                                                           std::move(deserializer),
                                                           _packetsCollector,
                                                           _consumer->GetRtpParameters());
        }
        InvokeObserverMethod(&ConsumerObserver::OnConsumerMediaStreamStarted, true);
    }
}

void ConsumerTranslator::WriteMediaPayload(uint32_t ssrc,
                                           const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (buffer) {
        LOCK_READ_PROTECTED_OBJ(_mediaGrabber);
        if (const auto& mediaGrabber = _mediaGrabber.ConstRef()) {
            LOCK_READ_PROTECTED_OBJ(_producerPacketsInfo);
            mediaGrabber->ProcessMediaPayload(ssrc, buffer, _producerPacketsInfo.ConstRef());
        }
    }
}

void ConsumerTranslator::EndMediaWriting()
{
    MediaSink::EndMediaWriting();
    bool ended = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_mediaGrabber);
        if (auto mediaGrabber = _mediaGrabber.Take()) {
            ended = true;
        }
    }
    if (ended) {
        InvokeObserverMethod(&ConsumerObserver::OnConsumerMediaStreamStarted, false);
    }
}

void ConsumerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ConsumerObserver::OnConsumerPauseChanged, pause);
}

template <class Method, typename... Args>
void ConsumerTranslator::InvokeObserverMethod(const Method& method, Args&&... args) const
{
    _observers.InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

ConsumerTranslator::MediaGrabber::MediaGrabber(Media::Kind kind,
                                               std::unique_ptr<MediaFrameDeserializer> deserializer,
                                               RtpPacketsCollector* packetsCollector,
                                               const RtpParameters& rtpParameters)
    : _audio(Media::Kind::AUDIO == kind)
    , _deserializer(std::move(deserializer))
    , _packetsCollector(packetsCollector)
    , _rtpParameters(rtpParameters)
{
}

void ConsumerTranslator::MediaGrabber::ProcessMediaPayload(uint32_t ssrc,
                                                           const std::shared_ptr<const MemoryBuffer>& payload,
                                                           const ProducerPacketsInfo& producerPacketsInfo)
{
    if (payload) {
        const auto result = _deserializer->AddBuffer(payload);
        bool requestMediaFrames = false;
        if (MaybeOk(result)) {
            requestMediaFrames = FetchMediaInfo(producerPacketsInfo);
        }
        else if (_lastProcessingError != result) {
            MS_ERROR_STD("failed to deserialize of input media buffer: %s", ToString(result));
        }
        if (requestMediaFrames && IsOk(result)) {
            DeserializeMediaFrames(ssrc);
        }
        _lastProcessingError = result;
    }
}

bool ConsumerTranslator::MediaGrabber::FetchMediaInfo(const ProducerPacketsInfo& producerPacketsInfo)
{
    if (_codecs.empty()) {
        if (const auto tracksCount = _deserializer->GetTracksCount()) {
            for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                const auto mime = _deserializer->GetTrackMimeType(trackIndex);
                if (mime.has_value() && mime->IsAudioCodec() == _audio) {
                    if (const auto codec = GetCodec(mime.value())) {
                        const auto pi = producerPacketsInfo.find(codec->payloadType);
                        if (pi != producerPacketsInfo.end()) {
                            const auto codecType = mime->GetSubtype();
                            std::unique_ptr<RtpPacketizer> packetizer;
                            switch (codecType) {
                                case RtpCodecMimeType::Subtype::OPUS:
                                    packetizer = std::make_unique<RtpPacketizerOpus>();
                                    break;
                                default:
                                    MS_ASSERT(false, "packetizer for [%s] not yet implemented", mime->ToString().c_str());
                                    break;
                            }
                            if (packetizer) {
                                const auto& packetInfo = pi->second;
                                _deserializer->SetInitialTimestamp(trackIndex, std::get<0U>(packetInfo));
                                _codecs[codecType] = std::make_unique<CodecInfo>(std::move(packetizer),
                                                                                 codec->payloadType,
                                                                                 trackIndex,
                                                                                 std::get<1U>(packetInfo),
                                                                                 std::get<2U>(packetInfo));
                            }
                        }
                        else {
                            // TODO: log warning
                        }
                    }
                    else {
                        // TODO: log warning
                    }
                }
            }
        }
    }
    return !_codecs.empty();
}

void ConsumerTranslator::MediaGrabber::DeserializeMediaFrames(uint32_t ssrc)
{
    for (auto it = _codecs.begin(); it != _codecs.end(); ++it) {
        MediaFrameDeserializeResult result;
        // TODO: rewrite access to RtpMemoryBufferPacket::GetPayloadOffset() with consider of packetizer class logic
        for (const auto& frame : _deserializer->ReadNextFrames(it->second->_trackIndex,
                                                               RtpMemoryBufferPacket::GetPayloadOffset(),
                                                               &result)) {
            if (const auto packet = it->second->_packetizer->AddFrame(frame)) {
                packet->SetPayloadType(it->second->_payloadType);
                packet->SetSsrc(ssrc);
                packet->SetSequenceNumber(++it->second->_sequenceNumber);
                _packetsCollector->AddPacket(packet);
                //delete packet;
            }
        }
        if (!MaybeOk(result) && it->second->_lastError != result) {
            MS_ERROR("failed to read deserialized %s frames: %s",
                     MimeSubTypeToString(it->first).c_str(), ToString(result));
        }
        it->second->_lastError = result;
    }
}

const RtpCodecParameters* ConsumerTranslator::MediaGrabber::GetCodec(const RtpCodecMimeType& mime) const
{
    for (const auto& codec : _rtpParameters.codecs) {
        if (codec.mimeType == mime) {
            return &codec;
        }
    }
    return nullptr;
}

const char* ConsumerTranslator::MediaGrabber::ToString(MediaFrameDeserializeResult result)
{
    switch (result) {
        case MediaFrameDeserializeResult::ParseError:
            return "parse error";
        case MediaFrameDeserializeResult::OutOfMemory:
            return "out of memory";
        case MediaFrameDeserializeResult::InvalidArg:
            return "invalid argument";
        case MediaFrameDeserializeResult::Success:
            return "success";
        case MediaFrameDeserializeResult::NeedMoreData:
            return "need more data";
        default:
            MS_ASSERT(false, "unknown media result enum value");
            break;
    }
    return "";
}

} // namespace RTC

namespace {

CodecInfo::CodecInfo(std::unique_ptr<RTC::RtpPacketizer> packetizer,
                     uint8_t payloadType, size_t trackIndex,
                     uint32_t ssrc, uint16_t sequenceNumber)
    : _packetizer(std::move(packetizer))
    , _payloadType(payloadType)
    , _trackIndex(trackIndex)
    , _ssrc(ssrc)
    , _sequenceNumber(sequenceNumber)
{
    MS_ASSERT(_packetizer, "packetizer must not be null");
}

}
