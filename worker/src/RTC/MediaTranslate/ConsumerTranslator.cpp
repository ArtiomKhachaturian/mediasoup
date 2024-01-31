#define MS_CLASS "RTC::ConsumerTranslator"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpMemoryBufferPacket.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpPacketsInfoProvider.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"

namespace {

struct CodecInfo
{
    const std::unique_ptr<RTC::RtpPacketizer> _packetizer;
    const size_t _trackIndex;
    uint16_t _sequenceNumber = 0U;
    RTC::MediaFrameDeserializeResult _lastError = RTC::MediaFrameDeserializeResult::Success;
    CodecInfo(std::unique_ptr<RTC::RtpPacketizer> packetizer, size_t trackIndex);
};

}

namespace RTC
{

class ConsumerTranslator::MediaGrabber
{
public:
    MediaGrabber(Media::Kind kind,
                 std::unique_ptr<MediaFrameDeserializer> deserializer,
                 RtpPacketsCollector* packetsCollector);
    void ProcessMediaPayload(uint32_t ssrc,
                             const std::shared_ptr<const MemoryBuffer>& payload,
                             const RtpPacketsInfoProvider* packetsInfoProvider);
private:
    bool FetchMediaInfo();
    void DeserializeMediaFrames(uint32_t ssrc, const RtpPacketsInfoProvider* packetsInfoProvider);
    static const char* ToString(MediaFrameDeserializeResult result);
private:
    const bool _audio;
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    RtpPacketsCollector* const _packetsCollector;
    absl::flat_hash_map<RtpCodecMimeType::Subtype, std::unique_ptr<CodecInfo>> _codecs;
    MediaFrameDeserializeResult _lastProcessingError = MediaFrameDeserializeResult::Success;
};

ConsumerTranslator::ConsumerTranslator(const Consumer* consumer,
                                       RtpPacketsCollector* packetsCollector,
                                       const RtpPacketsInfoProvider* packetsInfoProvider,
                                       const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory)
    : _consumer(consumer)
    , _packetsCollector(packetsCollector)
    , _packetsInfoProvider(packetsInfoProvider)
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
    std::atomic_store(&_mediaGrabber, std::shared_ptr<MediaGrabber>());
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
    return nullptr != std::atomic_load(&_mediaGrabber);
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
        auto mediaGrabber = std::make_shared<MediaGrabber>(_consumer->GetKind(),
                                                           std::move(deserializer),
                                                           _packetsCollector);
        std::atomic_store(&_mediaGrabber, std::move(mediaGrabber));
        InvokeObserverMethod(&ConsumerObserver::OnConsumerMediaStreamStarted, true);
    }
}

void ConsumerTranslator::WriteMediaPayload(uint32_t ssrc,
                                           const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (buffer) {
        if (const auto mediaGrabber = std::atomic_load(&_mediaGrabber)) {
            mediaGrabber->ProcessMediaPayload(ssrc, buffer, _packetsInfoProvider);
        }
    }
}

void ConsumerTranslator::EndMediaWriting()
{
    MediaSink::EndMediaWriting();
    if (std::atomic_exchange(&_mediaGrabber, std::shared_ptr<MediaGrabber>())) {
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
                                               RtpPacketsCollector* packetsCollector)
    : _audio(Media::Kind::AUDIO == kind)
    , _deserializer(std::move(deserializer))
    , _packetsCollector(packetsCollector)
{
}

void ConsumerTranslator::MediaGrabber::ProcessMediaPayload(uint32_t ssrc,
                                                           const std::shared_ptr<const MemoryBuffer>& payload,
                                                           const RtpPacketsInfoProvider* packetsInfoProvider)
{
    if (payload) {
        const auto result = _deserializer->AddBuffer(payload);
        bool requestMediaFrames = false;
        if (MaybeOk(result)) {
            requestMediaFrames = FetchMediaInfo();
        }
        else if (_lastProcessingError != result) {
            MS_ERROR_STD("failed to deserialize of input media buffer: %s", ToString(result));
        }
        if (requestMediaFrames && IsOk(result)) {
            DeserializeMediaFrames(ssrc, packetsInfoProvider);
        }
        _lastProcessingError = result;
    }
}

bool ConsumerTranslator::MediaGrabber::FetchMediaInfo()
{
    if (_codecs.empty()) {
        if (const auto tracksCount = _deserializer->GetTracksCount()) {
            for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                const auto mime = _deserializer->GetTrackMimeType(trackIndex);
                if (mime.has_value() && mime->IsAudioCodec() == _audio) {
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
                        auto codec = std::make_unique<CodecInfo>(std::move(packetizer), trackIndex);
                        _codecs[codecType] = std::move(codec);
                    }
                }
            }
        }
    }
    return !_codecs.empty();
}

void ConsumerTranslator::MediaGrabber::DeserializeMediaFrames(uint32_t ssrc,
                                                              const RtpPacketsInfoProvider* packetsInfoProvider)
{
    if (packetsInfoProvider) {
        for (auto it = _codecs.begin(); it != _codecs.end(); ++it) {
            if (!it->second->_sequenceNumber) {
                it->second->_sequenceNumber = packetsInfoProvider->GetLastOriginalRtpSeqNumber(ssrc);
                _deserializer->SetInitialTimestamp(it->second->_trackIndex, packetsInfoProvider->GetLastOriginalRtpTimestamp(ssrc));
                _deserializer->SetClockRate(it->second->_trackIndex, packetsInfoProvider->GetClockRate(ssrc));
            }
            MediaFrameDeserializeResult result;
            // TODO: rewrite access to RtpMemoryBufferPacket::GetPayloadOffset() with consider of packetizer class logic
            for (const auto& frame : _deserializer->ReadNextFrames(it->second->_trackIndex,
                                                                   &result)) {
                if (const auto packet = it->second->_packetizer->AddFrame(frame)) {
                    packet->SetPayloadType(packetsInfoProvider->GetPayloadType(ssrc));
                    packet->SetSsrc(ssrc);
                    packet->SetSequenceNumber(++it->second->_sequenceNumber);
                    _packetsCollector->AddPacket(packet);
                }
            }
            if (!MaybeOk(result) && it->second->_lastError != result) {
                MS_ERROR("failed to read deserialized %s frames: %s",
                         MimeSubTypeToString(it->first).c_str(), ToString(result));
            }
            it->second->_lastError = result;
        }
    }
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

CodecInfo::CodecInfo(std::unique_ptr<RTC::RtpPacketizer> packetizer, size_t trackIndex)
    : _packetizer(std::move(packetizer))
    , _trackIndex(trackIndex)
{
    MS_ASSERT(_packetizer, "packetizer must not be null");
}

}
