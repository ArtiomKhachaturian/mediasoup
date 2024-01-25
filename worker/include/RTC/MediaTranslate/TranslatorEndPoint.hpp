#pragma once

#include "FBS/translationPack.h"
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "ProtectedObj.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace RTC
{

class MediaSource;
class ConsumerTranslatorSettings;
class Websocket;

class TranslatorEndPoint : private WebsocketListener, private MediaSink
{
    class Impl;
public:
    TranslatorEndPoint(const std::string& serviceUri,
                       const std::string& serviceUser = std::string(),
                       const std::string& servicePassword = std::string(),
                       const std::string& userAgent = std::string());
    ~TranslatorEndPoint();
    void SetProducerLanguage(const std::optional<FBS::TranslationPack::Language>& language);
    void SetConsumerLanguageAndVoice(const std::optional<FBS::TranslationPack::Language>& language,
                                     const std::optional<FBS::TranslationPack::Voice>& voice);
    void SetInput(MediaSource* input);
    bool HasInput() const;
    void SetOutput(MediaSink* output);
    bool IsConnected() const { return _connected.load(std::memory_order_relaxed); }
private:
    static std::string JsonToString(const nlohmann::json& data);
    static std::string_view LanguageToId(const std::optional<FBS::TranslationPack::Language>& language);
    static std::string_view VoiceToId(FBS::TranslationPack::Voice voice);
    static nlohmann::json TargetLanguageCmd(FBS::TranslationPack::Language languageTo,
                                            FBS::TranslationPack::Voice voice,
                                            const std::optional<FBS::TranslationPack::Language>& languageFrom = std::nullopt);
    bool HasValidTranslationSettings() const;
    std::optional<FBS::TranslationPack::Language> GetConsumerLanguage() const;
    std::optional<FBS::TranslationPack::Voice> GetConsumerVoice() const;
    std::optional<FBS::TranslationPack::Language> GetProducerLanguage() const;
    void SetConnected(bool connected);
    void ConnectToMediaInput(bool connect);
    void ConnectToMediaInput(MediaSource* input, bool connect);
    void UpdateTranslationChanges();
    bool SendTranslationChanges();
    bool WriteJson(const nlohmann::json& data) const;
    void OpenSocket();
    // impl. of MediaSink
    void WriteMediaPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept final;
    // impl. of WebsocketListener
    void OnFailed(uint64_t socketId, FailureType type, const std::string& what);
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
    const std::string _userAgent;
    const std::unique_ptr<Websocket> _socket;
    const std::string _serviceUri; // for logs
    const uint32_t _startTimestamp;
    ProtectedOptional<FBS::TranslationPack::Language> _consumerLanguage;
    ProtectedOptional<FBS::TranslationPack::Voice> _consumerVoice;
    ProtectedOptional<FBS::TranslationPack::Language> _producerLanguage;
    ProtectedObj<MediaSource*> _input;
    ProtectedObj<MediaSink*> _output;
    std::atomic_bool _connected = false;
    bool _mediaRestarted = false;
};

} // namespace RTC
