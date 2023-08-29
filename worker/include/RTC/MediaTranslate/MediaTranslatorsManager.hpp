#ifndef MS_RTC_MEDIA_TRANSLATORS_MANAGER_HPP
#define MS_RTC_MEDIA_TRANSLATORS_MANAGER_HPP

#include "common.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"

namespace RTC
{

class RtpPacketsCollector;
class ProducerTranslator;

class MediaTranslatorsManager
{
    class MediaPacketsCollector;
    class Producer;
    class ProducerObserver;
    class ConsumerObserver;
    class Impl;
public:
    MediaTranslatorsManager();
    ~MediaTranslatorsManager();
    std::shared_ptr<ProducerTranslator> RegisterProducer(const std::string& producerId);
    void UnegisterProducer(const std::string& producerId);
private:
    const std::shared_ptr<Impl> _impl;
    //absl::flat_hash_map<std::string, std::unique_ptr<ProducerData>> _producers;
};

} // namespace RTC

#endif
