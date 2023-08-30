#pragma once

#include <memory>
#include <string>

namespace RTC
{

class ProducerTranslator;
class ConsumerTranslator;
class RtpPacketsCollector;
class Websocket;

class TranslatorEndPoint
{
    class Impl;
public:
    TranslatorEndPoint(uint32_t audioSsrc,
                       const std::weak_ptr<ProducerTranslator>& producerRef,
                       const std::weak_ptr<ConsumerTranslator>& consumerRef,
                       const std::string& serviceUri,
                       const std::string& serviceUser = std::string(),
                       const std::string& servicePassword = std::string());
    ~TranslatorEndPoint();
    uint32_t GetAudioSsrc() const;
    const std::string& GetProducerId() const;
    const std::string& GetConsumerId() const;
    bool Open(const std::string& userAgent = std::string());
    void Close();
    void SetOutput(RtpPacketsCollector* output);
    void SendTranslationChanges();
private:
    const std::shared_ptr<Websocket> _websocket;
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC
