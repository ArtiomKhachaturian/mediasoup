#pragma once

#include "FBS/translationPack.h"
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "ProtectedObj.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

#define WRITE_TRANSLATION_TO_FILE

namespace RTC
{

class ConsumerTranslatorSettings;
class MediaSource;
class Websocket;

class TranslatorEndPoint : private WebsocketListener, private MediaSink
{
    class InputSliceBuffer;
public:
    TranslatorEndPoint(const std::string& serviceUri,
                       const std::string& serviceUser = std::string(),
                       const std::string& servicePassword = std::string(),
                       const std::string& userAgent = std::string(),
                       uint32_t timeSliceMs = 200U);
    ~TranslatorEndPoint();
    void SetProducerLanguage(const std::optional<FBS::TranslationPack::Language>& language);
    void SetConsumerLanguageAndVoice(const std::optional<FBS::TranslationPack::Language>& language,
                                     const std::optional<FBS::TranslationPack::Voice>& voice);
    void SetInput(MediaSource* input);
    bool HasInput() const;
    void SetOutput(MediaSink* output);
    bool IsConnected() const;
private:
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
    bool WriteBinary(const MemoryBuffer* buffer) const;
    void OpenSocket();
    // impl. of MediaSink
    bool IsLiveMode() const final { return true; }
    void StartMediaWriting(uint32_t ssrc) final;
    void WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting() final;
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
    const std::string _userAgent;
    const std::unique_ptr<Websocket> _socket;
    const std::string _serviceUri; // for logs
    const std::unique_ptr<InputSliceBuffer> _inputSlice;
    ProtectedOptional<FBS::TranslationPack::Language> _consumerLanguage;
    ProtectedOptional<FBS::TranslationPack::Voice> _consumerVoice;
    ProtectedOptional<FBS::TranslationPack::Language> _producerLanguage;
    ProtectedObj<MediaSource*> _input;
    ProtectedObj<MediaSink*> _output;
    std::atomic<uint32_t> _ssrc = 0U;
#ifdef WRITE_TRANSLATION_TO_FILE
    std::atomic<uint32_t> _translationsCounter = 0ULL;
#endif
};

} // namespace RTC
