#pragma once
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "ProtectedObj.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

#define PLAY_MOCK_FILE_AFTER_CONNECTION
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
                       uint32_t timeSliceMs = 400U);
    ~TranslatorEndPoint();
    void SetInput(MediaSource* input);
    bool HasInput() const;
    void SetOutput(MediaSink* output);
    void SetInputLanguageId(const std::string& languageId);
    void SetOutputLanguageId(const std::string& languageId);
    void SetOutputVoiceId(const std::string& voiceId);
    bool IsConnected() const;
private:
    static nlohmann::json TargetLanguageCmd(const std::string& inputLanguageId,
                                            const std::string& outputLanguageId,
                                            const std::string& outputVoiceId);
    static void SendDataToMediaSink(uint32_t ssrc, const std::shared_ptr<MemoryBuffer>& data,
                                    MediaSink* sink);
    void ChangeTranslationSettings(const std::string& to, ProtectedObj<std::string>& object);
    bool HasValidTranslationSettings() const;
    std::string GetInputLanguageId() const;
    std::string GetOutputLanguageId() const;
    std::string GetOutputVoiceId() const;
    bool HasInputLanguageId() const;
    bool HasOutputLanguageId() const;
    bool HasOutputVoiceId() const;
    std::optional<nlohmann::json> TargetLanguageCmd() const;
    void SetConnected(bool connected);
    void ConnectToMediaInput(bool connect);
    void ConnectToMediaInput(MediaSource* input, bool connect);
    void UpdateTranslationChanges();
    bool SendTranslationChanges();
    bool WriteJson(const nlohmann::json& data) const;
    bool WriteBinary(const MemoryBuffer& buffer) const;
    void OpenSocket();
    void CloseSocket();
    // impl. of MediaSink
    void StartMediaWriting(uint32_t ssrc) final;
    void WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting(uint32_t ssrc) final;
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
#ifdef PLAY_MOCK_FILE_AFTER_CONNECTION
    static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm";
#endif
    const std::string _userAgent;
    const std::unique_ptr<Websocket> _socket;
    const std::string _serviceUri; // for logs
    const std::unique_ptr<InputSliceBuffer> _inputSlice;
    ProtectedObj<std::string> _inputLanguageId;
    ProtectedObj<std::string> _outputLanguageId;
    ProtectedObj<std::string> _outputVoiceId;
    ProtectedObj<MediaSource*> _input;
    ProtectedObj<MediaSink*> _output;
    std::atomic<uint32_t> _ssrc = 0U;
#ifdef WRITE_TRANSLATION_TO_FILE
    std::atomic<uint32_t> _translationsCounter = 0ULL;
#endif
};

} // namespace RTC
