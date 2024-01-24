#define MS_CLASS "RTC::ConsumerTranslator"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

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

std::optional<FBS::TranslationPack::Language> ConsumerTranslator::GetLanguage() const
{
    return _consumer->GetLanguage();
}

std::optional<FBS::TranslationPack::Voice> ConsumerTranslator::GetVoice() const
{
    return _consumer->GetVoice();
}

void ConsumerTranslator::StartMediaWriting(bool restart, uint32_t startTimestamp) noexcept
{
    MediaSink::StartMediaWriting(restart, startTimestamp);
    _deserializer = _serializationFactory->CreateDeserializer();
    if (_deserializer) {
        _deserializer->SetInitialTimestamp(startTimestamp);
    }
}

void ConsumerTranslator::WriteMediaPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer && _deserializer) {
        const auto result = _deserializer->AddBuffer(buffer);
        switch (result) {
            case MediaFrameDeserializeResult::Success:
            case MediaFrameDeserializeResult::NeedMoreData:
                FetchMediaTrackIndex();
                break;
            default:
                break;
        }
        if (MediaFrameDeserializeResult::Success == result) {
            // parse frames
            DeserializeMediaFrames();
        }
    }
}

void ConsumerTranslator::EndMediaWriting() noexcept
{
    MediaSink::EndMediaWriting();
    _deserializer.reset();
    _deserializedMediaTrackIndex = std::nullopt;
    _packetizers.clear();
}

void ConsumerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ConsumerObserver::OnConsumerPauseChanged, pause);
}

bool ConsumerTranslator::IsAudio() const
{
    return RTC::Media::Kind::AUDIO == _consumer->GetKind();
}

void ConsumerTranslator::FetchMediaTrackIndex()
{
    if (_deserializer && !_deserializedMediaTrackIndex.has_value()) {
        if (const auto tracksCount = _deserializer->GetTracksCount()) {
            for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                const auto mime = _deserializer->GetTrackMimeType(trackIndex);
                if (mime.has_value() && mime->IsAudioCodec() == IsAudio()) {
                    _deserializedMediaTrackIndex = trackIndex;
                    if (const auto codec = GetCodec(mime.value())) {
                        _deserializer->SetClockRate(trackIndex, codec->clockRate);
                    }
                    break;
                }
            }
        }
    }
}

void ConsumerTranslator::DeserializeMediaFrames()
{
    if (_deserializer && _deserializedMediaTrackIndex.has_value()) {
        std::vector<std::shared_ptr<const MediaFrame>> frames;
        _deserializer->ReadNextFrames(_deserializedMediaTrackIndex.value(), frames);
        for (const auto& frame : frames) {
            if (const auto packetizer = GetPacketizer(frame)) {
                if (const auto packet = packetizer->AddFrame(frame)) {
                    SetupRtpPacketParameters(frame->GetMimeType(), packet);
                    _packetsCollector->AddPacket(packet);
                    delete packet;
                }
            }
        }
    }
}

const RtpCodecParameters* ConsumerTranslator::GetCodec(const RtpCodecMimeType& mime) const
{
    for (const auto& codec : _consumer->GetRtpParameters().codecs) {
        if (codec.mimeType == mime) {
            return &codec;
        }
    }
    return nullptr;
}

void ConsumerTranslator::SetupRtpPacketParameters(const RtpCodecMimeType& mime, RtpPacket* packet) const
{
    if (packet) {
        if (const auto codec = GetCodec(mime)) {
            packet->SetPayloadType(codec->payloadType);
        }
    }
}

RtpPacketizer* ConsumerTranslator::GetPacketizer(const std::shared_ptr<const MediaFrame>& frame)
{
    if (frame) {
        const auto subType = frame->GetMimeType().GetSubtype();
        auto it = _packetizers.find(subType);
        if (it == _packetizers.end()) {
            std::unique_ptr<RtpPacketizer> packetizer;
            switch (subType) {
                case RtpCodecMimeType::Subtype::OPUS:
                    packetizer = std::make_unique<RtpPacketizerOpus>();
                    break;
                default:
                    MS_ASSERT(false, "packetizer for [%s] not yet implemented",
                              frame->GetMimeType().ToString().c_str());
                    break;
            }
            if (packetizer) {
                it = _packetizers.insert(std::make_pair(subType, std::move(packetizer))).first;
            }
        }
        if (it != _packetizers.end()) {
            return it->second.get();
        }
    }
    return nullptr;
}

template <class Method, typename... Args>
void ConsumerTranslator::InvokeObserverMethod(const Method& method, Args&&... args) const
{
    _observers.InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

} // namespace RTC
