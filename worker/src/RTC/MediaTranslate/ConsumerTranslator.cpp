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
    if (0 == deserializerSource->Open("/Users/user/Downloads/big-buck-bunny_trailer.webm")) {
        _deserializerSource = std::move(deserializerSource);
    }
#else
    _deserializerSource = std::make_unique<WebMBuffersReader>();
#endif
    if (_deserializerSource) {
        _deserializer = std::make_unique<RtpWebMDeserializer>(_deserializerSource.get());
    }
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
        if (_deserializer->Update()) {
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
            // parse frames
            if (const auto frame = _deserializer->ReadNextFrame(_deserializedMediaInfo->second)) {
                
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
