#define MS_CLASS "RTC::MediaFileWriter"
#include "RTC/MediaTranslate/MediaFileWriter.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaFileWriter::MediaFileWriter(FILE* file, uint32_t ssrc,
                                 std::unique_ptr<RtpMediaFrameSerializer> serializer,
                                 const std::shared_ptr<RtpDepacketizer>& depacketizer,
                                 bool liveMode)
    : FileWriter(file)
    , _ssrc(ssrc)
    , _serializer(std::move(serializer))
    , _depacketizer(depacketizer)
{
    MS_ASSERT(_serializer, "no media serializer");
    MS_ASSERT(_depacketizer, "no media depacketizer");
    if (_serializer) {
        _serializer->SetOutputDevice(this);
        _serializer->SetLiveMode(liveMode);
    }
}

MediaFileWriter::~MediaFileWriter()
{
    if (_serializer) {
        _serializer->SetOutputDevice(nullptr);
    }
    Close();
}

std::shared_ptr<MediaFileWriter> MediaFileWriter::Create(std::string_view outputFolderNameUtf8,
                                                         const RTC::RtpCodecMimeType& mime,
                                                         uint32_t ssrc, bool liveMode,
                                                         int* fileOpenError)
{
    if (!outputFolderNameUtf8.empty()) {
        if (auto serializer = RtpMediaFrameSerializer::create(mime)) {
            if (const auto depacketizer = RtpDepacketizer::create(mime)) {
                const auto filename = FormatMediaFileName(std::move(outputFolderNameUtf8), mime, ssrc);
                FILE* file = FileOpen(filename, fileOpenError);
                if (file) {
                    return std::make_unique<MediaFileWriter>(file, ssrc,
                                                             std::move(serializer),
                                                             depacketizer, liveMode);
                }
            }
        }
    }
    return nullptr;
}

void MediaFileWriter::SetTargetPacketsCollector(RtpPacketsCollector* targetPacketsCollector)
{
    if (targetPacketsCollector != this) {
        _targetPacketsCollector = targetPacketsCollector;
    }
}

void MediaFileWriter::AddPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet)
{
    if (packet && _serializer && _depacketizer && packet->GetSsrc() == _ssrc &&
        _depacketizer->GetCodecMimeType() == mimeType) {
        _serializer->Push(_depacketizer->AddPacket(packet));
    }
    if (_targetPacketsCollector) {
        _targetPacketsCollector->AddPacket(mimeType, packet);
    }
}

std::string MediaFileWriter::FormatMediaFileName(const RTC::RtpCodecMimeType& mime, uint32_t ssrc)
{
    const auto& type = RTC::RtpCodecMimeType::type2String[mime.type];
    return type + std::to_string(ssrc) + "." + RTC::RtpCodecMimeType::subtype2String[mime.subtype];
}

std::string MediaFileWriter::FormatMediaFileName(std::string_view outputFolderNameUtf8,
                                                 const RTC::RtpCodecMimeType& mime, uint32_t ssrc)
{
    if (!outputFolderNameUtf8.empty()) {
        return std::string(outputFolderNameUtf8.data(), outputFolderNameUtf8.size()) +
            "/" + FormatMediaFileName(mime, ssrc);
    }
    return std::string();
}

} // namespace RTC
