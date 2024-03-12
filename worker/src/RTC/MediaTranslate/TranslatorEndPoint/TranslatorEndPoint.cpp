#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointSink.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/Buffers/SegmentsBuffer.hpp"
#include "Logger.hpp"
#include <chrono>
#include <inttypes.h>

namespace RTC
{

class TranslatorEndPoint::InputSlice
{
    using SliceTimestamp = std::chrono::time_point<std::chrono::system_clock>;
public:
    InputSlice(uint32_t timeSliceMs, const std::shared_ptr<BufferAllocator>& allocator);
    std::shared_ptr<Buffer> Add(const std::shared_ptr<Buffer>& inputBuffer);
    void Reset(bool start);
    static std::unique_ptr<InputSlice> Create(uint32_t timeSliceMs,
                                              const std::shared_ptr<BufferAllocator>& allocator);
private:
    const std::chrono::milliseconds _timeSliceMs;
    Protected<SegmentsBuffer> _buffer;
    SliceTimestamp _sliceOriginTimestamp;
};

TranslatorEndPoint::TranslatorEndPoint(std::string ownerId, std::string name,
                                       const std::shared_ptr<BufferAllocator>& allocator,
                                       uint32_t timeSliceMs)
    : _inputSlice(InputSlice::Create(timeSliceMs, allocator))
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
    if (HasOutputVoiceId()) {
        const auto input = GetInputLanguageId();
        const auto output = GetOutputLanguageId();
        return !input.empty() && !output.empty() && input != output;
    }
    return false;
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
        MS_DEBUG_DEV_STD("Received translation #%" PRIu64 " at %s from %s", number,
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

nlohmann::json TranslatorEndPoint::TargetLanguageCmd(std::string inputLanguageId,
                                                     std::string outputLanguageId,
                                                     std::string outputVoiceId)
{
    // language settings
    nlohmann::json languageSettings;
    languageSettings["from"] = std::move(inputLanguageId);
    languageSettings["to"] = std::move(outputLanguageId);
    languageSettings["voiceID"] = std::move(outputVoiceId);
    // command
    nlohmann::json command;
    command["type"] = "set_target_language";
    command["cmd"] = std::move(languageSettings);
    return command;
}

void TranslatorEndPoint::ChangeTranslationSettings(std::string to, ProtectedString& object)
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

nlohmann::json TranslatorEndPoint::TargetLanguageCmd() const
{
    auto input = GetInputLanguageId();
    if (!input.empty()) {
        auto output = GetOutputLanguageId();
        if (!output.empty()) {
            auto voice = GetOutputVoiceId();
            if (!voice.empty()) {
                return TargetLanguageCmd(std::move(input), std::move(output), std::move(voice));
            }
        }
    }
    return {};
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
            if (!input->AddSink(this)) {
                MS_ERROR_STD("unable connect translation service %s to media input",
                             GetDescription().c_str());
            }
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
        const auto cmd = TargetLanguageCmd();
        if (!cmd.empty()) {
            return WriteJson(cmd);
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
    _outputMediaSinks.InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

void TranslatorEndPoint::StartMediaWriting(uint64_t senderId)
{
    MediaSink::StartMediaWriting(senderId);
    if (_inputSlice) {
        _inputSlice->Reset(true);
    }
}

void TranslatorEndPoint::WriteMediaPayload(uint64_t, const std::shared_ptr<Buffer>& buffer)
{
    if (buffer && !buffer->IsEmpty() && IsConnected()) {
        if (_inputSlice) {
            if (const auto outputBuffer = _inputSlice->Add(buffer)) {
                WriteBinary(outputBuffer);
            }
        }
        else {
            WriteBinary(buffer);
        }
    }
}

void TranslatorEndPoint::EndMediaWriting(uint64_t senderId)
{
    MediaSink::EndMediaWriting(senderId);
    if (_inputSlice) {
        _inputSlice->Reset(false);
    }
}

TranslatorEndPoint::InputSlice::InputSlice(uint32_t timeSliceMs,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    : _timeSliceMs(timeSliceMs)
    , _buffer(allocator)
{
}

std::shared_ptr<Buffer> TranslatorEndPoint::InputSlice::Add(const std::shared_ptr<Buffer>& inputBuffer)
{
    std::shared_ptr<Buffer> outputBuffer;
    if (inputBuffer) {
        LOCK_WRITE_PROTECTED_OBJ(_buffer);
        if (SegmentsBuffer::Result::Failed != _buffer->Push(inputBuffer)) {
            auto now = std::chrono::system_clock::now();
            if (now > _sliceOriginTimestamp + _timeSliceMs) {
                _sliceOriginTimestamp = std::move(now);
                outputBuffer = _buffer->Take();
            }
        }
        else {
            MS_ERROR_STD("unable to add memory buffer (%zu bytes) to input slice",
                         inputBuffer->GetSize());
        }
    }
    return outputBuffer;
}

void TranslatorEndPoint::InputSlice::Reset(bool start)
{
    LOCK_WRITE_PROTECTED_OBJ(_buffer);
    _buffer->Clear();
    if (start) {
        _sliceOriginTimestamp = std::chrono::system_clock::now();
    }
    else {
        _sliceOriginTimestamp = SliceTimestamp();
    }
}

std::unique_ptr<TranslatorEndPoint::InputSlice> TranslatorEndPoint::InputSlice::
    Create(uint32_t timeSliceMs, const std::shared_ptr<BufferAllocator>& allocator)
{
    if (timeSliceMs) {
        return std::make_unique<InputSlice>(timeSliceMs, allocator);
    }
    return nullptr;
}

} // namespace RTC
