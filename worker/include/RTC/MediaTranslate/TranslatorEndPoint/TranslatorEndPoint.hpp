#pragma once
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/MediaSourceImpl.hpp"
#include "ProtectedObj.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace RTC
{

class MediaSource;

class TranslatorEndPoint : public MediaSourceImpl, private MediaSink
{
    class InputSliceBuffer;
    using ProtectedString = ProtectedObj<std::string>;
public:
    ~TranslatorEndPoint() override;
    void SetInputMediaSource(MediaSource* inputMediaSource);
    void SetInputLanguageId(const std::string& languageId);
    void SetOutputLanguageId(const std::string& languageId);
    void SetOutputVoiceId(const std::string& voiceId);
    virtual bool IsConnected() const = 0;
protected:
    TranslatorEndPoint(std::string ownerId = std::string(), std::string name = std::string(),
                       uint32_t timeSliceMs = 0U);
    bool HasInput() const;
    bool HasValidTranslationSettings() const;
    void NotifyThatConnectionEstablished(bool connected);
    void NotifyThatTranslatedMediaReceived(const std::shared_ptr<MemoryBuffer>& media);
    const std::string& GetOwnerId() const { return _ownerId; }
    const std::string& GetName() const { return _name; }
    std::string GetDescription() const;
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;
    virtual bool SendBinary(const MemoryBuffer& buffer) const = 0;
    virtual bool SendText(const std::string& text) const = 0;
    // override of MediaSourceImpl
    bool IsSinkValid(const MediaSink* sink) const final { return this != sink; }
    void OnSinkWasAdded(MediaSink* sink, bool first) final;
    void OnSinkWasRemoved(MediaSink* sink, bool last) final;
private:
    static nlohmann::json TargetLanguageCmd(const std::string& inputLanguageId,
                                            const std::string& outputLanguageId,
                                            const std::string& outputVoiceId);
    void ChangeTranslationSettings(const std::string& to, ProtectedObj<std::string>& object);
    bool CanConnect() const { return HasInput() && HasSinks() && HasValidTranslationSettings(); }
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
    void StartMediaWriting(const MediaObject& sender) final;
    void WriteMediaPayload(const MediaObject& sender, const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting(const MediaObject& sender) final;
private:
    const std::unique_ptr<InputSliceBuffer> _inputSlice;
    const std::string _ownerId;
    const std::string _name;
    ProtectedString _inputLanguageId;
    ProtectedString _outputLanguageId;
    ProtectedString _outputVoiceId;
    ProtectedObj<MediaSource*> _inputMediaSource;
    std::atomic_bool _notifyedThatConnected = false; // for logs
};

} // namespace RTC
