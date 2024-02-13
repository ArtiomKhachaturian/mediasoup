#pragma once
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "ProtectedObj.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace RTC
{

class MediaSource;
class TranslatorEndPointListener;

class TranslatorEndPoint : private MediaSink
{
    class InputSliceBuffer;
public:
    virtual ~TranslatorEndPoint();
    void SetInput(MediaSource* input);
    void SetOutput(TranslatorEndPointListener* output);
    void SetInputLanguageId(const std::string& languageId);
    void SetOutputLanguageId(const std::string& languageId);
    void SetOutputVoiceId(const std::string& voiceId);
    uint32_t GetSsrc() const { return _ssrc; }
    virtual bool IsConnected() const = 0;
protected:
    TranslatorEndPoint(uint32_t ssrc, uint32_t timeSliceMs);
    bool HasInput() const;
    bool HasOutput() const;
    bool HasValidTranslationSettings() const;
    void NotifyThatConnectionEstablished(bool connected);
    void NotifyThatTranslatedMediaReceived(const std::shared_ptr<MemoryBuffer>& media);
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;
    virtual bool SendBinary(const MemoryBuffer& buffer) const = 0;
    virtual bool SendText(const std::string& text) const = 0;
private:
    static nlohmann::json TargetLanguageCmd(const std::string& inputLanguageId,
                                            const std::string& outputLanguageId,
                                            const std::string& outputVoiceId);
    void ChangeTranslationSettings(const std::string& to, ProtectedObj<std::string>& object);
    bool CanConnect() const { return HasInput() && HasOutput() && HasValidTranslationSettings(); }
    std::string GetInputLanguageId() const;
    std::string GetOutputLanguageId() const;
    std::string GetOutputVoiceId() const;
    bool HasInputLanguageId() const;
    bool HasOutputLanguageId() const;
    bool HasOutputVoiceId() const;
    std::optional<nlohmann::json> TargetLanguageCmd() const;
    void ConnectToMediaInput(bool connect);
    void ConnectToMediaInput(MediaSource* input, bool connect);
    void UpdateTranslationChanges();
    bool SendTranslationChanges();
    bool WriteJson(const nlohmann::json& data) const;
    bool WriteBinary(const MemoryBuffer& buffer) const;
    // impl. of MediaSink
    void StartMediaWriting(uint32_t ssrc) final;
    void WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting(uint32_t ssrc) final;
private:
    static inline std::atomic_uint64_t _receivedMediaCounter = 0ULL;
    const uint32_t _ssrc;
    const std::unique_ptr<InputSliceBuffer> _inputSlice;
    ProtectedObj<std::string> _inputLanguageId;
    ProtectedObj<std::string> _outputLanguageId;
    ProtectedObj<std::string> _outputVoiceId;
    ProtectedObj<MediaSource*> _input;
    ProtectedObj<TranslatorEndPointListener*> _output;
};

} // namespace RTC