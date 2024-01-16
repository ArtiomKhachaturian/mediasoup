#define MS_CLASS "RTC::ConsumerTranslator"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameDeserializer.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"

namespace RTC
{

ConsumerTranslator::ConsumerTranslator(const Consumer* consumer,
                                       RtpPacketsCollector* packetsCollector,
                                       std::unique_ptr<RtpMediaFrameDeserializer> deserializer)
    : _consumer(consumer)
    , _packetsCollector(packetsCollector)
    , _deserializer(std::move(deserializer))
{
    MS_ASSERT(_consumer, "consumer must not be null");
    MS_ASSERT(_packetsCollector, "RTP packets collector must not be null");
    MS_ASSERT(_deserializer, "packets deserializer must not be null");
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

void ConsumerTranslator::Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer && _deserializer->AddBuffer(buffer)) {
        if (!_deserializedMediaInfo) {
            if (const auto tracksCount = _deserializer->GetTracksCount()) {
                for (size_t i = 0UL; i < tracksCount; ++i) {
                    auto mime = _deserializer->GetTrackMimeType(i);
                    if (mime.has_value() && mime->IsAudioCodec() == IsAudio()) {
                        _deserializedMediaInfo = std::make_pair(std::move(mime.value()), i);
                        break;
                    }
                }
            }
        }
        else { // parse frames
            
        }
    }
}

void ConsumerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ConsumerObserver::OnConsumerPauseChanged, pause);
}

bool ConsumerTranslator::IsAudio() const
{
    return RTC::Media::Kind::AUDIO == _consumer->GetKind();
}

template <class Method, typename... Args>
void ConsumerTranslator::InvokeObserverMethod(const Method& method, Args&&... args) const
{
    _observers.InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

} // namespace RTC
