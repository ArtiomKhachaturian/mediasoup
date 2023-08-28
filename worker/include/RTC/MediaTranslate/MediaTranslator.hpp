#ifndef MS_RTC_MEDIA_TRANSLATOR_HPP
#define MS_RTC_MEDIA_TRANSLATOR_HPP

#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/RtpDictionaries.hpp"

#include <memory>
#include <atomic>

namespace RTC
{

class RtpMediaFrameSerializer;
class RtpDepacketizer;
class Websocket;

class MediaTranslator : public RtpPacketsCollector
{
    class Impl;
public:
    enum class Language
    {
        English,
        Italian,
        Spain,
        Thai,
        French,
        German,
        Russian,
        Arabic,
        Farsi
    };
    enum class Voice
    {
        Abdul,
        Jesus_Rodriguez,
        Test_Irina,
        Serena,
        Ryan
    };
public:
    MediaTranslator(const std::string& serviceUri,
                    std::unique_ptr<RtpMediaFrameSerializer> serializer,
                    std::unique_ptr<RtpDepacketizer> depacketizer);
    ~MediaTranslator() final;
    static std::unique_ptr<MediaTranslator> Create(std::string serviceUri,
                                                   const RtpCodecMimeType& mimeType);
    bool OpenService(const std::string& user = std::string(), const std::string& password = std::string());
    void CloseService();
    void SetFromLanguage(Language language);
    void SetFromLanguageAuto();
    void SetToLanguage(Language language);
    void SetVoice(Voice voice);
    // impl. of RtpPacketsCollector
    void AddPacket(const RtpPacket* packet) final;
private:
    static std::unique_ptr<Websocket> CreateWebsocket(const std::string& serviceUri);
    static std::shared_ptr<Impl> CreateImpl(Websocket* websocket);
private:
    const std::unique_ptr<RtpMediaFrameSerializer> _serializer;
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    const std::unique_ptr<Websocket> _websocket;
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC

#endif
