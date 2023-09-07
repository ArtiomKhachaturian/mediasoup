#define MS_CLASS "RTC::ConsumerTranslator"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"

namespace RTC
{

ConsumerTranslator::ConsumerTranslator(Consumer* consumer,
                                       const std::string& producerId,
                                       const std::string& serviceUri,
                                       const std::string& serviceUser,
                                       const std::string& servicePassword)
    : _consumer(consumer)
    , _producerId(producerId)
    , _endPoint(std::make_unique<TranslatorEndPoint>(serviceUri, serviceUser, servicePassword))
{
    MS_ASSERT(_consumer, "consumer must not be null");
    MS_ASSERT(!_producerId.empty(), "producer ID must not be empty");
    if (IsEnabled()) {
        _endPoint->Open();
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
    if (observer) {
        if (_observers.end() == std::find(_observers.begin(), _observers.end(), observer)) {
            _observers.push_back(observer);
        }
    }
}

void ConsumerTranslator::RemoveObserver(ConsumerObserver* observer)
{
    if (observer) {
        const auto it = std::find(_observers.begin(), _observers.end(), observer);
        if (it != _observers.end()) {
            _observers.erase(it);
        }
    }
}

void ConsumerTranslator::SetProducerInput(const std::shared_ptr<ProducerInputMediaStreamer>& input)
{
    _endPoint->SetInput(input);
}

void ConsumerTranslator::SetProducerLanguage(const std::optional<MediaLanguage>& language)
{
    _endPoint->SetProducerLanguage(language);
}

void ConsumerTranslator::SetLanguage(MediaLanguage language)
{
    if (language != _language) {
        const auto from = _language;
        _language = language;
        InvokeObserverMethod(&ConsumerObserver::OnConsumerLanguageChanged, from, language);
        _endPoint->SetConsumerLanguage(language);
    }
}

void ConsumerTranslator::SetVoice(MediaVoice voice)
{
    if (voice != _voice) {
        const auto from = _voice;
        _voice = voice;
        InvokeObserverMethod(&ConsumerObserver::OnConsumerVoiceChanged, from, voice);
        _endPoint->SetConsumerVoice(voice);
    }
}

void ConsumerTranslator::SetEnabled(bool enabled)
{
    if (enabled != _enabled) {
        _enabled = enabled;
        InvokeObserverMethod(&ConsumerObserver::OnConsumerEnabledChanged, enabled);
        if (enabled) {
            _endPoint->Open();
        }
        else {
            _endPoint->Close();
        }
    }
}

void ConsumerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ConsumerObserver::OnConsumerPauseChanged, pause);
}

template <class Method, typename... Args>
void ConsumerTranslator::InvokeObserverMethod(const Method& method, Args&&... args) const
{
    if (method) {
        for (const auto observer : _observers) {
            ((*observer).*method)(GetId(), std::forward<Args>(args)...);
        }
    }
}

} // namespace RTC
