#pragma once
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "ProtectedObj.hpp"
#include "RTC/ObjectId.hpp"
#include "RTC/Listeners.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <optional>
#include <string>

namespace RTC
{

class TranslatorEndPointSink;
class MediaSource;

class TranslatorEndPoint : public ObjectId, private MediaSink
{
    class InputSliceBuffer;
    using ProtectedString = ProtectedObj<std::string>;
public:
    virtual ~TranslatorEndPoint() override;
    // in/out media connections
    void SetInputMediaSource(MediaSource* inputMediaSource);
    bool AddOutputMediaSink(TranslatorEndPointSink* sink);
    bool RemoveOutputMediaSink(TranslatorEndPointSink* sink);
    bool HasOutputMediaSinks() const { return !_outputMediaSinks.IsEmpty(); }
    // language settings
    void SetInputLanguageId(const std::string& languageId);
    void SetOutputVoiceId(const std::string& voiceId);
    void SetOutputLanguageId(const std::string& languageId);
    std::string GetInputLanguageId() const;
    std::string GetOutputLanguageId() const;
    std::string GetOutputVoiceId() const;
    // other properties
    const std::string& GetOwnerId() const { return _ownerId; }
    const std::string& GetName() const { return _name; }
    virtual bool IsConnected() const = 0;
protected:
    TranslatorEndPoint(std::string ownerId = std::string(),
                       std::string name = std::string(),
                       uint32_t timeSliceMs = 0U);
    bool HasInput() const;
    bool HasValidTranslationSettings() const;
    void NotifyThatConnectionEstablished(bool connected);
    uint64_t NotifyThatTranslationReceived(const std::shared_ptr<Buffer>& media);
    std::string GetDescription() const;
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;
    virtual bool SendBinary(const std::shared_ptr<Buffer>& buffer) const = 0;
    virtual bool SendText(const std::string& text) const = 0;
private:
    static nlohmann::json TargetLanguageCmd(const std::string& inputLanguageId,
                                            const std::string& outputLanguageId,
                                            const std::string& outputVoiceId);
    void ChangeTranslationSettings(const std::string& to, ProtectedObj<std::string>& object);
    bool CanConnect() const;
    bool HasInputLanguageId() const;
    bool HasOutputLanguageId() const;
    bool HasOutputVoiceId() const;
    std::optional<nlohmann::json> TargetLanguageCmd() const;
    void ConnectToMediaInput(bool connect);
    void ConnectToMediaInput(MediaSource* input, bool connect);
    void UpdateTranslationChanges();
    bool SendTranslationChanges();
    bool WriteJson(const nlohmann::json& data) const;
    bool WriteBinary(const std::shared_ptr<Buffer>& buffer) const;
    template <class Method, typename... Args>
    void InvokeOutputMediaSinks(const Method& method, Args&&... args) const;
    // impl. of MediaSink
    void StartMediaWriting(const ObjectId& sender) final;
    void WriteMediaPayload(const ObjectId& sender, const std::shared_ptr<Buffer>& buffer) final;
    void EndMediaWriting(const ObjectId& sender) final;
private:
    const std::unique_ptr<InputSliceBuffer> _inputSlice;
    const std::string _ownerId;
    const std::string _name;
    ProtectedString _inputLanguageId;
    ProtectedString _outputLanguageId;
    ProtectedString _outputVoiceId;
    ProtectedObj<MediaSource*> _inputMediaSource = nullptr;
    Listeners<TranslatorEndPointSink*> _outputMediaSinks;
    std::atomic_bool _notifyedThatConnected = false; // for logs
    std::atomic<uint64_t> _translationsCount = 0ULL;
};

} // namespace RTC
