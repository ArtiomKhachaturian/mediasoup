#pragma once

#include <memory>
#include <string>
#include <optional>

namespace RTC
{

class ProducerInputMediaStreamer;
class ConsumerTranslatorSettings;
class RtpPacketsCollector;
class Websocket;
enum class MediaLanguage;
enum class MediaVoice;

class TranslatorEndPoint
{
    class Impl;
public:
    TranslatorEndPoint(const std::string& serviceUri,
                       const std::string& serviceUser = std::string(),
                       const std::string& servicePassword = std::string(),
                       const std::string& userAgent = std::string());
    ~TranslatorEndPoint();
    void Open();
    void Close();
    void SetProducerLanguage(const std::optional<MediaLanguage>& language);
    void SetConsumerLanguage(MediaLanguage language);
    void SetConsumerVoice(MediaVoice voice);
    void SetInput(const std::shared_ptr<ProducerInputMediaStreamer>& input);
    void SetOutput(const std::weak_ptr<RtpPacketsCollector>& outputRef);
private:
    const std::shared_ptr<Websocket> _websocket;
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC
