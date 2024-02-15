#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointListener.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"

namespace RTC
{

class TranslatorEndPoint::InputSliceBuffer
{
public:
    InputSliceBuffer(uint32_t timeSliceMs);
    void Add(const std::shared_ptr<MemoryBuffer>& buffer, TranslatorEndPoint* endPoint);
    void Reset(bool start);
    static std::unique_ptr<InputSliceBuffer> Create(uint32_t timeSliceMs);
private:
    const uint32_t _timeSliceMs;
    uint64_t _sliceOriginTimestamp = 0ULL;
    ProtectedObj<SimpleMemoryBuffer> _impl;
};

TranslatorEndPoint::TranslatorEndPoint(uint32_t ssrc, uint32_t timeSliceMs)
    : _ssrc(ssrc)
    , _inputSlice(InputSliceBuffer::Create(timeSliceMs))
{
    MS_ASSERT(_ssrc, "SSRC must be greater than zero");
}

TranslatorEndPoint::~TranslatorEndPoint()   
{
    NotifyThatConnectionEstablished(false);
}

void TranslatorEndPoint::SetInput(MediaSource* input)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_input);
        if (input != _input.ConstRef()) {
            ConnectToMediaInput(_input.ConstRef(), false);
            _input = input;
            if (IsConnected()) {
                ConnectToMediaInput(_input.ConstRef(), true);
            }
            changed = true;
        }
    }
    if (changed) {
        if (input) {
            if (CanConnect()) {
                Connect();
            }
        }
        else {
            Disconnect();
        }
    }
}

void TranslatorEndPoint::SetOutput(TranslatorEndPointListener* output)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_output);
        if (output != _output.ConstRef()) {
            _output = output;
            changed = true;
        }
    }
    if (changed) {
        if (output) {
            if (CanConnect()) {
                Connect();
            }
        }
        else {
            Disconnect();
        }
    }
}

void TranslatorEndPoint::SetInputLanguageId(const std::string& languageId)
{
    ChangeTranslationSettings(languageId, _inputLanguageId);
}

void TranslatorEndPoint::SetOutputLanguageId(const std::string& languageId)
{
    ChangeTranslationSettings(languageId, _outputLanguageId);
}

void TranslatorEndPoint::SetOutputVoiceId(const std::string& voiceId)
{
    ChangeTranslationSettings(voiceId, _outputVoiceId);
}

bool TranslatorEndPoint::HasInput() const
{
    LOCK_READ_PROTECTED_OBJ(_input);
    return nullptr != _input.ConstRef();
}

bool TranslatorEndPoint::HasOutput() const
{
    LOCK_READ_PROTECTED_OBJ(_output);
    return nullptr != _output.ConstRef();
}

bool TranslatorEndPoint::HasValidTranslationSettings() const
{
    return HasOutputLanguageId() && HasOutputLanguageId() && HasOutputVoiceId();
}

void TranslatorEndPoint::NotifyThatConnectionEstablished(bool connected)
{
    if (connected) {
        if (SendTranslationChanges()) {
            ConnectToMediaInput(true);
        }
    }
    else {
        ConnectToMediaInput(false);
        if (_inputSlice) {
            _inputSlice->Reset(false);
        }
    }
}

void TranslatorEndPoint::NotifyThatTranslatedMediaReceived(const std::shared_ptr<MemoryBuffer>& media)
{
    if (media) {
        const auto num = _receivedMediaCounter.fetch_add(1U);
        LOCK_READ_PROTECTED_OBJ(_output);
        if (const auto output = _output.ConstRef()) {
            output->OnTranslatedMediaReceived(this, num, media);
        }
    }
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

void TranslatorEndPoint::ChangeTranslationSettings(const std::string& to,
                                                   ProtectedObj<std::string>& object)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(object);
        if (to != object.ConstRef()) {
            object = to;
            changed = true;
        }
    }
    if (changed) {
        UpdateTranslationChanges();
    }
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

bool TranslatorEndPoint::HasInputLanguageId() const
{
    LOCK_READ_PROTECTED_OBJ(_inputLanguageId);
    return !_inputLanguageId->empty();
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
    LOCK_READ_PROTECTED_OBJ(_input);
    ConnectToMediaInput(_input.ConstRef(), connect);
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
    if (IsConnected()) {
        return SendText(nlohmann::to_string(data));
    }
    return false;
}

bool TranslatorEndPoint::WriteBinary(const MemoryBuffer& buffer) const
{
    if (IsConnected()) {
        return SendBinary(buffer);
    }
    return false;
}

void TranslatorEndPoint::StartMediaWriting()
{
    MediaSink::StartMediaWriting();
    if (_inputSlice) {
        _inputSlice->Reset(true);
    }
}

void TranslatorEndPoint::WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer && !buffer->IsEmpty() && IsConnected()) {
        if (_inputSlice) {
            _inputSlice->Add(buffer, this);
        }
        else {
            WriteBinary(*buffer);
        }
    }
}

void TranslatorEndPoint::EndMediaWriting()
{
    MediaSink::EndMediaWriting();
    if (_inputSlice) {
        _inputSlice->Reset(false);
    }
}

TranslatorEndPoint::InputSliceBuffer::InputSliceBuffer(uint32_t timeSliceMs)
    : _timeSliceMs(timeSliceMs)
{
}

void TranslatorEndPoint::InputSliceBuffer::Add(const std::shared_ptr<MemoryBuffer>& buffer,
                                               TranslatorEndPoint* endPoint)
{
    if (buffer && endPoint) {
        LOCK_WRITE_PROTECTED_OBJ(_impl);
        if (_impl->Append(buffer)) {
            const auto now = DepLibUV::GetTimeMs();
            if (now > _sliceOriginTimestamp + _timeSliceMs) {
                _sliceOriginTimestamp = now;
                endPoint->WriteBinary(_impl.ConstRef());
                _impl->Clear();
            }
        }
        else {
            MS_ERROR_STD("unable to add memory buffer (%zu bytes) to input slice", buffer->GetSize());
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
    Create(uint32_t timeSliceMs)
{
    if (timeSliceMs) {
        return std::make_unique<InputSliceBuffer>(timeSliceMs);
    }
    return nullptr;
}

} // namespace RTC
