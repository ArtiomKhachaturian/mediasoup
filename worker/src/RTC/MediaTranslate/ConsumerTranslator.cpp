#define MS_CLASS "RTC::ConsumerTranslator"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpPacket.hpp"
#include "DepLibUV.hpp"
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

void ConsumerTranslator::StartStream(bool restart) noexcept
{
    MediaSink::StartStream(restart);
    _deserializer = _serializationFactory->CreateDeserializer();
}

void ConsumerTranslator::WriteMediaPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer && _deserializer && _deserializer->AddBuffer(buffer)) {
        if (!_deserializedMediaTrackIndex.has_value()) {
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
        if (_deserializedMediaTrackIndex.has_value()) {
            // parse frames
            const auto frames = _deserializer->ReadNextFrames(_deserializedMediaTrackIndex.value());
            if (!frames.empty()) {
                _hadIncomingMedia = true;
                for (const auto& frame : frames) {
                    if (const auto packetizer = GetPacketizer(frame)) {
                        if (const auto packet = packetizer->AddFrame(frame)) {
                            SetupRtpPacketParameters(frame->GetMimeType(), packet);
                            _packetsCollector->AddPacket(packet);
                        }
                    }
                }
            }
        }
    }
}

void ConsumerTranslator::EndStream(bool failure) noexcept
{
    MediaSink::EndStream(failure);
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
        switch (mime.GetSubtype()) {
            case RtpCodecMimeType::Subtype::OPUS:
                Codecs::Opus::ProcessRtpPacket(packet);
                break;
            default:
                break;
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
