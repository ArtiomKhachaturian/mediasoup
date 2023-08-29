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
enum class MediaVoice;
enum class MediaLanguage;

// primitve (draft implementation)
class MediaTranslator : public RtpPacketsCollector
{
    class Impl;
public:
    MediaTranslator(const std::string& serviceUri,
                    std::shared_ptr<RtpMediaFrameSerializer> serializer,
                    std::unique_ptr<RtpDepacketizer> depacketizer);
    ~MediaTranslator() final;
    static std::unique_ptr<MediaTranslator> Create(std::string serviceUri,
                                                   const RtpCodecMimeType& mimeType);
    bool OpenService(const std::string& user = std::string(), const std::string& password = std::string());
    void CloseService();
    void SetFromLanguage(MediaLanguage language);
    void SetFromLanguageAuto();
    void SetToLanguage(MediaLanguage language);
    void SetVoice(MediaVoice voice);
    // impl. of RtpPacketsCollector
    void AddPacket(const RTC::RtpCodecMimeType& mimeType, const RtpPacket* packet) final;
private:
    static std::unique_ptr<Websocket> CreateWebsocket(const std::string& serviceUri);
    static std::shared_ptr<Impl> CreateImpl(Websocket* websocket);
private:
    const std::shared_ptr<RtpMediaFrameSerializer> _serializer;
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    const std::unique_ptr<Websocket> _websocket;
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC

#endif
