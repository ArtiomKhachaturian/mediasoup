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
    _outputDevices.Add(outputDevice);
}

void RtpMediaFrameSerializer::RemoveOutputDevice(OutputDevice* outputDevice)
{
    _outputDevices.Remove(outputDevice);
}

bool RtpMediaFrameSerializer::HasDevices() const
{
    return !_outputDevices.IsEmpty();
}

void RtpMediaFrameSerializer::StartStream(bool restart) noexcept
{
    _outputDevices.InvokeMethod(&OutputDevice::StartStream, restart);
}

void RtpMediaFrameSerializer::BeginWriteMediaPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) noexcept
{
    if (mediaFrame) {
        const auto& packetsInfo = mediaFrame->GetPacketsInfo();
        if (!packetsInfo.empty()) {
            _outputDevices.InvokeMethod(&OutputDevice::BeginWriteMediaPayload,
                                        mediaFrame->GetSsrc(), packetsInfo);
        }
    }
}

void RtpMediaFrameSerializer::WritePayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer) {
        _outputDevices.InvokeMethod(&OutputDevice::Write, buffer);
    }
}

void RtpMediaFrameSerializer::EndWriteMediaPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame,
                                                   bool ok) noexcept
{
    if (mediaFrame) {
        const auto& packetsInfo = mediaFrame->GetPacketsInfo();
        if (!packetsInfo.empty()) {
            _outputDevices.InvokeMethod(&OutputDevice::EndWriteMediaPayload, mediaFrame->GetSsrc(),
                                        packetsInfo, ok);
        }
    }
}

void RtpMediaFrameSerializer::EndStream(bool failure) noexcept
{
    _outputDevices.InvokeMethod(&OutputDevice::EndStream, failure);
}

} // namespace RTC
