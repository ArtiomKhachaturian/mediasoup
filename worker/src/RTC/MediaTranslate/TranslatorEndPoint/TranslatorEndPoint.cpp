#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointSink.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/Buffers/SimpleBuffer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include <inttypes.h>

namespace RTC
{

class TranslatorEndPoint::InputSliceBuffer
{
public:
    InputSliceBuffer(TranslatorEndPoint* owner, uint32_t timeSliceMs);
    void Add(const std::shared_ptr<Buffer>& buffer);
    void Reset(bool start);
    static std::unique_ptr<InputSliceBuffer> Create(TranslatorEndPoint* owner,
                                                    uint32_t timeSliceMs);
private:
    TranslatorEndPoint* const _owner;
    const uint32_t _timeSliceMs;
    uint64_t _sliceOriginTimestamp = 0ULL;
    ProtectedObj<SimpleBuffer> _impl;
};

TranslatorEndPoint::TranslatorEndPoint(std::string ownerId, std::string name, uint32_t timeSliceMs)
    : _inputSlice(InputSliceBuffer::Create(this, timeSliceMs))
    , _ownerId(std::move(ownerId))
    , _name(std::move(name))
{
}

TranslatorEndPoint::~TranslatorEndPoint()   
{
    NotifyThatConnectionEstablished(false);
}

void TranslatorEndPoint::SetInputMediaSource(MediaSource* inputMediaSource)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_inputMediaSource);
        if (inputMediaSource != _inputMediaSource.ConstRef()) {
            ConnectToMediaInput(_inputMediaSource.ConstRef(), false);
            _inputMediaSource = inputMediaSource;
            if (IsConnected()) {
                ConnectToMediaInput(_inputMediaSource.ConstRef(), true);
            }
            changed = true;
        }
    }
    if (changed) {
        if (inputMediaSource) {
            if (CanConnect()) {
                Connect();
            }
        }
        else {
            Disconnect();
        }
    }
}

bool TranslatorEndPoint::AddOutputMediaSink(TranslatorEndPointSink* sink)
{
    if (_outputMediaSinks.Add(sink)) {
        if (1U == _outputMediaSinks.GetSize() && CanConnect()) {
            Connect();
        }
        return true;
    }
    return false;
}

bool TranslatorEndPoint::RemoveOutputMediaSink(TranslatorEndPointSink* sink)
{
    if (_outputMediaSinks.Remove(sink)) {
        if (_outputMediaSinks.IsEmpty()) {
            Disconnect();
        }
        return true;
    }
    return false;
}

void TranslatorEndPoint::RemoveAllOutputMediaSinks()
{
    if (!_outputMediaSinks.IsEmpty()) {
        _outputMediaSinks.Clear();
        Disconnect();
    }
}

void TranslatorEndPoint::SetInputLanguageId(std::string languageId)
{
    ChangeTranslationSettings(std::move(languageId), _inputLanguageId);
}

void TranslatorEndPoint::SetOutputLanguageId(std::string languageId)
{
    ChangeTranslationSettings(std::move(languageId), _outputLanguageId);
}

void TranslatorEndPoint::SetOutputVoiceId(std::string voiceId)
{
    ChangeTranslationSettings(std::move(voiceId), _outputVoiceId);
}

std::string TranslatorEndPoint::GetInputLanguageId() const
{
    LOCK_READ_PROTECTED_OBJ(_inputLanguageId);
    return _inputLanguageId.ConstRef();
}

std::string TranslatorEndPoint::GetOutputLanguageId() const
{
    LOCK_READ_PROTECTED_OBJ(_outputLanguageId);
    return _outputLanguageId.ConstRef();
}

std::string TranslatorEndPoint::GetOutputVoiceId() const
{
    LOCK_READ_PROTECTED_OBJ(_outputVoiceId);
    return _outputVoiceId.ConstRef();
}

bool TranslatorEndPoint::HasInput() const
{
    LOCK_READ_PROTECTED_OBJ(_inputMediaSource);
    return nullptr != _inputMediaSource.ConstRef();
}

bool TranslatorEndPoint::HasValidTranslationSettings() const
{
    return HasOutputLanguageId() && HasOutputLanguageId() && HasOutputVoiceId();
}

void TranslatorEndPoint::NotifyThatConnectionEstablished(bool connected)
{
    if (connected != _notifyedThatConnected.exchange(connected)) {
        if (connected) {
            if (SendTranslationChanges()) {
                ConnectToMediaInput(true);
            }
            MS_DEBUG_DEV_STD("Connected to %s", GetDescription().c_str());
        }
        else {
            ConnectToMediaInput(false);
            MS_DEBUG_DEV_STD("Disconnected from %s", GetDescription().c_str());
        }
        InvokeOutputMediaSinks(&TranslatorEndPointSink::NotifyThatConnectionEstablished, connected);
    }
}

uint64_t TranslatorEndPoint::NotifyThatTranslationReceived(const std::shared_ptr<Buffer>& media)
{
    if (media) {
        const auto number = _translationsCount.fetch_add(1U) + 1U;
        MS_DEBUG_DEV_STD("Received translation #%" PRIu64 "at %s from %s", number,
                         GetCurrentTime().c_str(),
                         GetDescription().c_str());
        InvokeOutputMediaSinks(&MediaSink::StartMediaWriting);
        InvokeOutputMediaSinks(&MediaSink::WriteMediaPayload, media);
        InvokeOutputMediaSinks(&MediaSink::EndMediaWriting);
        return number;
    }
    return 0UL;
}

std::string TranslatorEndPoint::GetDescription() const
{
    auto name = GetName();
    if (!name.empty()) {
        auto ownerId = GetOwnerId();
        if (!ownerId.empty()) {
            name += " (" + ownerId + ")";
        }
        return name;
    }
    return "<anonymous end-point>";
}

bool TranslatorEndPoint::IsInputMediaSourcePaused() const
{
    LOCK_READ_PROTECTED_OBJ(_inputMediaSource);
    const auto inputMediaSource = _inputMediaSource.ConstRef();
    return inputMediaSource && inputMediaSource->IsPaused();
}

nlohmann::json TranslatorEndPoint::TargetLanguageCmd(const std::string& inputLanguageId,
                                                     const std::string& outputLanguageId,
                                                     const std::string& outputVoiceId)
{
    // language settings
    nlohmann::json languageSettings;
    languageSettings["from"] = inputLanguageId;
    languageSettings["to"] = outputLanguageId;
    languageSettings["voiceID"] = outputVoiceId;
    // command
    nlohmann::json command;
    command["type"] = "set_target_language";
    command["cmd"] = languageSettings;
    return command;
}

void TranslatorEndPoint::ChangeTranslationSettings(std::string to, ProtectedObj<std::string>& object)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(object);
        if (to != object.ConstRef()) {
            object = std::move(to);
            changed = true;
        }
    }
    if (changed) {
        UpdateTranslationChanges();
    }
}

bool TranslatorEndPoint::HasInputLanguageId() const
{
    LOCK_READ_PROTECTED_OBJ(_inputLanguageId);
    return !_inputLanguageId->empty();
}

bool TranslatorEndPoint::CanConnect() const
{
    return HasInput() && HasOutputMediaSinks() && HasValidTranslationSettings();
}

bool TranslatorEndPoint::HasOutputLanguageId() const
{
    LOCK_READ_PROTECTED_OBJ(_outputLanguageId);
    return !_outputLanguageId->empty();
}

bool TranslatorEndPoint::HasOutputVoiceId() const
{
    LOCK_READ_PROTECTED_OBJ(_outputVoiceId);
    return !_outputVoiceId->empty();
}

std::optional<nlohmann::json> TranslatorEndPoint::TargetLanguageCmd() const
{
    const auto inputLanguageId = GetInputLanguageId();
    if (!inputLanguageId.empty()) {
        const auto outputLanguageId = GetOutputLanguageId();
        if (!outputLanguageId.empty()) {
            const auto outputVoiceId = GetOutputVoiceId();
            if (!outputVoiceId.empty()) {
                return TargetLanguageCmd(inputLanguageId, outputLanguageId, outputVoiceId);
            }
        }
    }
    return std::nullopt;
}

void TranslatorEndPoint::ConnectToMediaInput(bool connect)
{
    if (!connect && _inputSlice) {
        _inputSlice->Reset(false);
    }
    LOCK_READ_PROTECTED_OBJ(_inputMediaSource);
    ConnectToMediaInput(_inputMediaSource.ConstRef(), connect);
}

void TranslatorEndPoint::ConnectToMediaInput(MediaSource* input, bool connect)
{
    if (input) {
        if (connect) {
            input->AddSink(this);
        }
        else {
            input->RemoveSink(this);
        }
    }
}

void TranslatorEndPoint::UpdateTranslationChanges()
{
    if (HasValidTranslationSettings()) {
        if (IsConnected()) {
            SendTranslationChanges();
        }
        else if (CanConnect()) {
            Connect();
        }
    }
    else {
        Disconnect();
    }
}

bool TranslatorEndPoint::SendTranslationChanges()
{
    if (IsConnected()) {
        if (const auto cmd = TargetLanguageCmd()) {
            return WriteJson(cmd.value());
        }
    }
    return false;
}

bool TranslatorEndPoint::WriteJson(const nlohmann::json& data) const
{
    bool ok = false;
    if (IsConnected()) {
        const auto text = nlohmann::to_string(data);
        ok = SendText(text);
        if (!ok) {
            MS_ERROR_STD("failed write JSON '%s' into translation service %s",
                         text.c_str(), GetDescription().c_str());
        }
    }
    return ok;
}

bool TranslatorEndPoint::WriteBinary(const std::shared_ptr<Buffer>& buffer) const
{
    bool ok = false;
    if (buffer && IsConnected()) {
        ok = SendBinary(buffer);
        if (!ok) {
            MS_ERROR_STD("failed write binary (%zu bytes)' into translation service %s",
                         buffer->GetSize(), GetDescription().c_str());
        }
    }
    return ok;
}

template <class Method, typename... Args>
void TranslatorEndPoint::InvokeOutputMediaSinks(const Method& method, Args&&... args) const
{
    _outputMediaSinks.InvokeMethod(method, *this, std::forward<Args>(args)...);
}

void TranslatorEndPoint::StartMediaWriting(const ObjectId& sender)
{
    MediaSink::StartMediaWriting(sender);
    if (_inputSlice) {
        _inputSlice->Reset(true);
    }
}

void TranslatorEndPoint::WriteMediaPayload(const ObjectId& /*sender*/,
                                           const std::shared_ptr<Buffer>& buffer)
{
    if (buffer && !buffer->IsEmpty() && IsConnected()) {
        if (_inputSlice) {
            _inputSlice->Add(buffer);
        }
        else {
            WriteBinary(buffer);
        }
    }
}

void TranslatorEndPoint::EndMediaWriting(const ObjectId& sender)
{
    MediaSink::EndMediaWriting(sender);
    if (_inputSlice) {
        _inputSlice->Reset(false);
    }
}

TranslatorEndPoint::InputSliceBuffer::InputSliceBuffer(TranslatorEndPoint* owner,
                                                       uint32_t timeSliceMs)
    : _owner(owner)
    , _timeSliceMs(timeSliceMs)
{
}

void TranslatorEndPoint::InputSliceBuffer::Add(const std::shared_ptr<Buffer>& buffer)
{
    if (buffer) {
        std::shared_ptr<Buffer> outputBuffer;
        {
            LOCK_WRITE_PROTECTED_OBJ(_impl);
            if (_impl->Append(buffer)) {
                const auto now = DepLibUV::GetTimeMs();
                if (now > _sliceOriginTimestamp + _timeSliceMs) {
                    _sliceOriginTimestamp = now;
                    outputBuffer = _impl->Take();
                }
            }
            else {
                MS_ERROR_STD("unable to add memory buffer (%zu bytes) to input slice", buffer->GetSize());
            }
        }
        if (outputBuffer) {
            _owner->WriteBinary(outputBuffer);
        }
    }
}

void TranslatorEndPoint::InputSliceBuffer::Reset(bool start)
{
    LOCK_WRITE_PROTECTED_OBJ(_impl);
    _impl->Clear();
    if (start) {
        _sliceOriginTimestamp = DepLibUV::GetTimeMs();
    }
    else {
        _sliceOriginTimestamp = 0ULL;
    }
}

std::unique_ptr<TranslatorEndPoint::InputSliceBuffer> TranslatorEndPoint::InputSliceBuffer::
    Create(TranslatorEndPoint* owner, uint32_t timeSliceMs)
{
    if (owner && timeSliceMs) {
        return std::make_unique<InputSliceBuffer>(owner, timeSliceMs);
    }
    return nullptr;
}

} // namespace RTC
