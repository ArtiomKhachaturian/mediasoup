#define MS_CLASS "RTC::ConsumerTranslator"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMBuffersReader.hpp"
#include "RTC/MediaTranslate/WebM/RtpWebMDeserializer.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"
#ifdef USE_TEST_FILE
#include <mkvparser/mkvreader.h>
#endif

namespace RTC
{

ConsumerTranslator::ConsumerTranslator(const Consumer* consumer,
                                       RtpPacketsCollector* packetsCollector)
    : _consumer(consumer)
    , _packetsCollector(packetsCollector)
{
    MS_ASSERT(_consumer, "consumer must not be null");
    MS_ASSERT(_packetsCollector, "RTP packets collector must not be null");
#ifdef USE_TEST_FILE
    auto deserializerSource = std::make_unique<mkvparser::MkvReader>();
    if (0 == deserializerSource->Open("/Users/user/Downloads/1b0cefc4-abdb-48d0-9c50-f5050755be94.webm")) {
        _deserializerSource = std::move(deserializerSource);
        _deserializer = std::make_unique<RtpWebMDeserializer>(_deserializerSource.get());
    }
#else
    _deserializerSource = std::make_unique<WebMBuffersReader>();
    _deserializer = std::make_unique<RtpWebMDeserializer>(_deserializerSource.get());
#endif
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
    if (buffer && _deserializer) {
#ifndef USE_TEST_FILE
        if (!_deserializerSource->AddBuffer(buffer)) {
            return;
        }
#endif
        if (!_deserializedMediaTrackIndex.has_value()) {
            if (_deserializer->Start()) {
                if (const auto tracksCount = _deserializer->GetTracksCount()) {
                    for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                        const auto mime = _deserializer->GetTrackMimeType(trackIndex);
                        if (mime.has_value() && mime->IsAudioCodec() == IsAudio()) {
                            _deserializedMediaTrackIndex = trackIndex;
                            break;
                        }
                    }
                }
            }
        }
        if (_deserializedMediaTrackIndex.has_value()) {
            // parse frames
            const auto frames = _deserializer->ReadNextFrames(_deserializedMediaTrackIndex.value());
            if (!frames.empty()) {
                
            }
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
