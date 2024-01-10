#pragma once

#include "FBS/translationPack.h"
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "ProtectedObj.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace RTC
{

class ProducerInputMediaStreamer;
class ConsumerTranslatorSettings;
class RtpPacketsCollector;
class Websocket;

class TranslatorEndPoint : private WebsocketListener, private OutputDevice
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
    void SetProducerLanguage(const std::optional<FBS::TranslationPack::Language>& language);
    void SetConsumerLanguageAndVoice(const std::optional<FBS::TranslationPack::Language>& language,
                                     const std::optional<FBS::TranslationPack::Voice>& voice);
    void SetInput(const std::shared_ptr<ProducerInputMediaStreamer>& input);
    bool HasInput() const;
    void SetOutput(const std::weak_ptr<RtpPacketsCollector>& outputRef);
private:
    static std::string_view LanguageToString(const std::optional<FBS::TranslationPack::Language>& language);
    static std::string_view VoiceToString(FBS::TranslationPack::Voice voice);
    static nlohmann::json TargetLanguageCmd(FBS::TranslationPack::Language languageTo,
                                            FBS::TranslationPack::Voice voice,
                                            const std::optional<FBS::TranslationPack::Language>& languageFrom = std::nullopt);
    bool IsConnected() const { return _connected.load(std::memory_order_relaxed); }
    bool IsWantsToOpen() const { return _wantsToOpen.load(std::memory_order_relaxed); }
    bool HasValidTranslationSettings() const;
    std::optional<FBS::TranslationPack::Language> GetConsumerLanguage() const;
    std::optional<FBS::TranslationPack::Voice> GetConsumerVoice() const;
    std::optional<FBS::TranslationPack::Language> GetProducerLanguage() const;
    void SetConnected(bool connected);
    void ConnectToMediaInput(bool connect);
    void ConnectToMediaInput(const std::shared_ptr<ProducerInputMediaStreamer>& input, bool connect);
    void UpdateTranslationChanges();
    bool SendTranslationChanges();
    bool WriteJson(const nlohmann::json& data) const;
    void OpenSocket();
    // impl. of OutputDevice
    void StartStream(bool restart) noexcept final;
    void BeginWriteMediaPayload(uint32_t ssrc,
                                const std::vector<RtpMediaPacketInfo>& packets) noexcept final;
    void EndWriteMediaPayload(uint32_t ssrc,
                              const std::vector<RtpMediaPacketInfo>& packets,
                              bool ok) noexcept final;
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept final;
    void EndStream(bool failure) noexcept final;
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnTextMessageReceived(uint64_t socketId, std::string message) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
    const std::string _userAgent;
    const std::unique_ptr<Websocket> _socket;
    std::atomic_bool _wantsToOpen = false;
    std::atomic_bool _connected = false;
    ProtectedOptional<FBS::TranslationPack::Language> _consumerLanguage;
    ProtectedOptional<FBS::TranslationPack::Voice> _consumerVoice;
    ProtectedOptional<FBS::TranslationPack::Language> _producerLanguage;
    ProtectedSharedPtr<ProducerInputMediaStreamer> _input;
    ProtectedWeakPtr<RtpPacketsCollector> _outputRef;
};

} // namespace RTC
