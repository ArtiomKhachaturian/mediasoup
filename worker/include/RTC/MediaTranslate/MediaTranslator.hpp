#pragma once

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
                    std::unique_ptr<RtpMediaFrameSerializer> serializer,
                    std::shared_ptr<RtpDepacketizer> depacketizer,
                    const std::string& user = std::string(),
                    const std::string& password = std::string());
    ~MediaTranslator() final;
    static std::unique_ptr<MediaTranslator> Create(std::string serviceUri,
                                                   const RtpCodecMimeType& mimeType,
                                                   const std::string& user = std::string(),
                                                   const std::string& password = std::string());
    bool OpenService();
    void CloseService();
    void SetFromLanguage(MediaLanguage language);
    void SetFromLanguageAuto();
    void SetToLanguage(MediaLanguage language);
    void SetVoice(MediaVoice voice);
    // impl. of RtpPacketsCollector
    void AddPacket(const RTC::RtpCodecMimeType& mimeType, const RtpPacket* packet) final;
private:
    static std::unique_ptr<Websocket> CreateWebsocket(const std::string& serviceUri,
                                                      const std::string& user,
                                                      const std::string& password);
    static std::shared_ptr<Impl> CreateImpl(Websocket* websocket);
private:
    const std::unique_ptr<RtpMediaFrameSerializer> _serializer;
    const std::shared_ptr<RtpDepacketizer> _depacketizer;
    const std::unique_ptr<Websocket> _websocket;
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC
