#define MS_CLASS "RTC::RtpMediaFrameSerializer"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "Logger.hpp"

namespace RTC
{

std::shared_ptr<RtpMediaFrameSerializer> RtpMediaFrameSerializer::create(const RtpCodecMimeType& mimeType)
{
    if (RtpWebMSerializer::IsSupported(mimeType)) {
        return std::make_shared<RtpWebMSerializer>();
    }
    return nullptr;
}

std::string_view RtpMediaFrameSerializer::GetFileExtension(const RtpCodecMimeType& mimeType) const
{
    return MimeSubTypeToString(mimeType.GetSubtype());
}

void RtpMediaFrameSerializer::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        LOCK_WRITE_PROTECTED_OBJ(_outputDevices);
        if (!_outputDevices->count(outputDevice)) {
            _outputDevices->insert(outputDevice);
        }
    }
}

void RtpMediaFrameSerializer::RemoveOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        LOCK_WRITE_PROTECTED_OBJ(_outputDevices);
        const auto it = _outputDevices->find(outputDevice);
        if (it != _outputDevices->end()) {
            _outputDevices->erase(it);
        }
    }
}

bool RtpMediaFrameSerializer::HasDevices() const
{
    LOCK_READ_PROTECTED_OBJ(_outputDevices);
    return !_outputDevices->empty();
}

void RtpMediaFrameSerializer::StartStream(bool restart) noexcept
{
    LOCK_READ_PROTECTED_OBJ(_outputDevices);
    for (const auto outputDevice : _outputDevices.ConstRef()) {
        outputDevice->StartStream(restart);
    }
}

void RtpMediaFrameSerializer::BeginWriteMediaPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) noexcept
{
    if (mediaFrame) {
        const auto& packetsInfo = mediaFrame->GetPacketsInfo();
        if (!packetsInfo.empty()) {
            const auto ssrc = mediaFrame->GetSsrc();
            LOCK_READ_PROTECTED_OBJ(_outputDevices);
            for (const auto outputDevice : _outputDevices.ConstRef()) {
                outputDevice->BeginWriteMediaPayload(ssrc, packetsInfo);
            }
        }
    }
}

void RtpMediaFrameSerializer::WritePayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer) {
        LOCK_READ_PROTECTED_OBJ(_outputDevices);
        for (const auto outputDevice : _outputDevices.ConstRef()) {
            outputDevice->Write(buffer);
        }
    }
}

void RtpMediaFrameSerializer::EndWriteMediaPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame,
                                                   bool ok) noexcept
{
    if (mediaFrame) {
        const auto& packetsInfo = mediaFrame->GetPacketsInfo();
        if (!packetsInfo.empty()) {
            const auto ssrc = mediaFrame->GetSsrc();
            LOCK_READ_PROTECTED_OBJ(_outputDevices);
            for (const auto outputDevice : _outputDevices.ConstRef()) {
                outputDevice->EndWriteMediaPayload(ssrc, packetsInfo, ok);
            }
        }
    }
}

void RtpMediaFrameSerializer::EndStream(bool failure) noexcept
{
    LOCK_READ_PROTECTED_OBJ(_outputDevices);
    for (const auto outputDevice : _outputDevices.ConstRef()) {
        outputDevice->EndStream(failure);
    }
}

} // namespace RTC
