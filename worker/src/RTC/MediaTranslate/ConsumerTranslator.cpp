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

using namespace RTC;

class MediaInfo
{
public:
    virtual ~MediaInfo() = default;
    MediaFrameDeserializeResult SetResult(uint32_t ssrc, MediaFrameDeserializeResult result,
                                          const char* operationName = "");
protected:
    MediaInfo(const RtpPacketsInfoProvider* packetsInfoProvider);
    const RtpPacketsInfoProvider* GetPacketsInfoProvider() const { return _packetsInfoProvider; }
    uint8_t GetPayloadType(uint32_t ssrc) const;
    uint16_t GetLastOriginalRtpSeqNumber(uint32_t ssrc) const;
    uint32_t GetLastOriginalRtpTimestamp(uint32_t ssrc) const;
    uint32_t GetClockRate(uint32_t ssrc) const;
    virtual std::string_view GetDescription() const { return ""; }
private:
    const RtpPacketsInfoProvider* const _packetsInfoProvider;
    std::atomic<MediaFrameDeserializeResult> _lastResult = MediaFrameDeserializeResult::Success;
};

}

namespace RTC
{

class ConsumerTranslator::MediaGrabber : public MediaInfo
{
public:
    MediaGrabber(uint32_t ssrc, Media::Kind kind,
                 std::unique_ptr<MediaFrameDeserializer> deserializer,
                 const std::vector<RtpCodecParameters>& codecParameters,
                 RtpPacketsCollector* packetsCollector,
                 const RtpPacketsInfoProvider* packetsInfoProvider);
    void ProcessMediaPayload(const std::shared_ptr<MemoryBuffer>& payload);
protected:
    // overrides of MediaInfo
    std::string_view GetDescription() const final { return _audio ? "audio grabber" : "video grabber"; }
private:
    bool FetchMediaInfo();
    void DeserializeMediaFrames();
    const RtpCodecParameters* GetCodecParameters(const RtpCodecMimeType& mime) const;
private:
    const uint32_t _ssrc;
    const bool _audio;
    const std::unique_ptr<MediaFrameDeserializer> _deserializer;
    const std::vector<RtpCodecParameters>& _codecParameters;
    RtpPacketsCollector* const _packetsCollector;
    absl::flat_hash_map<RtpCodecMimeType::Subtype, std::unique_ptr<CodecInfo>> _codecs;
};

class ConsumerTranslator::CodecInfo : public MediaInfo
{
public:
    CodecInfo(std::unique_ptr<RtpPacketizer> packetizer,
              RtpCodecMimeType::Subtype codecType, size_t trackIndex,
              const RtpPacketsInfoProvider* packetsInfoProvider);
    size_t GetTrackIndex() const { return _trackIndex; }
    bool HasSequenceNumber() const { return _sequenceNumber > 0U; }
    RtpPacket* AddFrame(uint32_t ssrc, const std::shared_ptr<const MediaFrame>& frame);
protected:
    // overrides of MediaInfo
    std::string_view GetDescription() const final;
private:
    const std::unique_ptr<RtpPacketizer> _packetizer;
    const RtpCodecMimeType::Subtype _codecType;
    const size_t _trackIndex;
    uint16_t _sequenceNumber = 0U;
    uint32_t _initialRtpTimestamp = 0U;
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
    MS_ASSERT(_packetsInfoProvider, "RTP packets info provider must not be null");
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

void ConsumerTranslator::StartMediaWriting(uint32_t ssrc)
{
    MediaSink::StartMediaWriting(ssrc);
    if (auto deserializer = _serializationFactory->CreateDeserializer()) {
        auto mediaGrabber = std::make_shared<MediaGrabber>(ssrc, _consumer->GetKind(),
                                                           std::move(deserializer),
                                                           _consumer->GetRtpParameters().codecs,
                                                           _packetsCollector,
                                                           _packetsInfoProvider);
        std::atomic_store(&_mediaGrabber, std::move(mediaGrabber));
        InvokeObserverMethod(&ConsumerObserver::OnConsumerMediaStreamStarted, true);
    }
}

void ConsumerTranslator::WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer) {
        if (const auto mediaGrabber = std::atomic_load(&_mediaGrabber)) {
            mediaGrabber->ProcessMediaPayload(buffer);
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

ConsumerTranslator::MediaGrabber::MediaGrabber(uint32_t ssrc, Media::Kind kind,
                                               std::unique_ptr<MediaFrameDeserializer> deserializer,
                                               const std::vector<RtpCodecParameters>& codecParameters,
                                               RtpPacketsCollector* packetsCollector,
                                               const RtpPacketsInfoProvider* packetsInfoProvider)
    : MediaInfo(packetsInfoProvider)
    , _ssrc(ssrc)
    , _audio(Media::Kind::AUDIO == kind)
    , _deserializer(std::move(deserializer))
    , _codecParameters(codecParameters)
    , _packetsCollector(packetsCollector)
{
}

void ConsumerTranslator::MediaGrabber::ProcessMediaPayload(const std::shared_ptr<MemoryBuffer>& payload)
{
    if (payload) {
        const auto result = SetResult(_ssrc, _deserializer->AddBuffer(payload),
                                      "input media buffer deserialization");
        bool requestMediaFrames = false;
        if (MaybeOk(result)) {
            requestMediaFrames = FetchMediaInfo();
        }
        if (requestMediaFrames && IsOk(result)) {
            DeserializeMediaFrames();
        }
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
                        auto codec = std::make_unique<CodecInfo>(std::move(packetizer),
                                                                 codecType, trackIndex,
                                                                 GetPacketsInfoProvider());
                        _codecs[codecType] = std::move(codec);
                    }
                }
            }
        }
    }
    return !_codecs.empty();
}

void ConsumerTranslator::MediaGrabber::DeserializeMediaFrames()
{
    for (auto it = _codecs.begin(); it != _codecs.end(); ++it) {
        auto& codec = it->second;
        if (!codec->HasSequenceNumber()) {
            _deserializer->SetClockRate(codec->GetTrackIndex(), GetClockRate(_ssrc));
        }
        MediaFrameDeserializeResult result = MediaFrameDeserializeResult::Success;
        for (const auto& frame : _deserializer->ReadNextFrames(codec->GetTrackIndex(), &result)) {
            if (const auto packet = codec->AddFrame(_ssrc, frame)) {
                if (const auto cp = GetCodecParameters(frame->GetMimeType())) {
                    packet->SetPayloadType(cp->payloadType);
                }
                _packetsCollector->AddPacket(packet);
            }
        }
        codec->SetResult(_ssrc, result, "read of deserialized frames");
    }
}

const RtpCodecParameters* ConsumerTranslator::MediaGrabber::GetCodecParameters(const RtpCodecMimeType& mime) const
{
    for (const auto& codecParameter : _codecParameters) {
        if (codecParameter.mimeType == mime) {
            return &codecParameter;
        }
    }
    return nullptr;
}

ConsumerTranslator::CodecInfo::CodecInfo(std::unique_ptr<RtpPacketizer> packetizer,
                                         RtpCodecMimeType::Subtype codecType, size_t trackIndex,
                                         const RtpPacketsInfoProvider* packetsInfoProvider)
    : MediaInfo(packetsInfoProvider)
    , _packetizer(std::move(packetizer))
    , _codecType(codecType)
    , _trackIndex(trackIndex)
{
    MS_ASSERT(_packetizer, "packetizer must not be null");
}

std::string_view ConsumerTranslator::CodecInfo::GetDescription() const
{
    return MimeSubTypeToString(_codecType).c_str();
}

RtpPacket* ConsumerTranslator::CodecInfo::AddFrame(uint32_t ssrc, const std::shared_ptr<const MediaFrame>& frame)
{
    if (frame) {
        if (const auto packet = _packetizer->AddFrame(frame)) {
            if (!_initialRtpTimestamp) {
                _initialRtpTimestamp = GetLastOriginalRtpTimestamp(ssrc);
            }
            _sequenceNumber = std::max(_sequenceNumber, GetLastOriginalRtpSeqNumber(ssrc));
            const uint32_t timestamp = _initialRtpTimestamp + frame->GetTimestamp();
            packet->SetSsrc(ssrc);
            packet->SetSequenceNumber(++_sequenceNumber);
            packet->SetTimestamp(std::max(timestamp, GetLastOriginalRtpTimestamp(ssrc)));
            return packet;
        }
    }
    return nullptr;
}

} // namespace RTC

namespace {

MediaInfo::MediaInfo(const RtpPacketsInfoProvider* packetsInfoProvider)
    : _packetsInfoProvider(packetsInfoProvider)
{
}

MediaFrameDeserializeResult MediaInfo::SetResult(uint32_t ssrc, MediaFrameDeserializeResult result,
                                                 const char* operationName)
{
    if (result != _lastResult.exchange(result) && !MaybeOk(result)) {
        MS_ERROR_STD("%s (SSRC = %u) operation %s failed: %s",
                     GetDescription().data(), ssrc, operationName,
                     ToString(result));
    }
    return result;
}

uint8_t MediaInfo::GetPayloadType(uint32_t ssrc) const
{
    return GetPacketsInfoProvider()->GetPayloadType(ssrc);
}

uint16_t MediaInfo::GetLastOriginalRtpSeqNumber(uint32_t ssrc) const
{
    return GetPacketsInfoProvider()->GetLastOriginalRtpSeqNumber(ssrc);
}

uint32_t MediaInfo::GetLastOriginalRtpTimestamp(uint32_t ssrc) const
{
    return GetPacketsInfoProvider()->GetLastOriginalRtpTimestamp(ssrc);
}

uint32_t MediaInfo::GetClockRate(uint32_t ssrc) const
{
    return GetPacketsInfoProvider()->GetClockRate(ssrc);
}

}
