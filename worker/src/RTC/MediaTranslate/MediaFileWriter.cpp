#define MS_CLASS "RTC::MediaFileWriter"
#include "RTC/MediaTranslate/MediaFileWriter.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "Logger.hpp"

namespace {

// 1st - file type, 2nd - extension
inline std::pair<std::string, std::string> GetFileNameMetrics(const RTC::RtpCodecMimeType& mime)
{
    if (RTC::RtpMediaFrame::IsValidMime(mime)) {
        const auto type = RTC::RtpCodecMimeType::type2String.find(mime.type);
        if (type !=  RTC::RtpCodecMimeType::type2String.end()) {
            const auto subtype = RTC::RtpCodecMimeType::subtype2String.find(mime.subtype);
            if (subtype != RTC::RtpCodecMimeType::subtype2String.end()) {
                return std::make_pair(type->second, subtype->second);
            }
        }
    }
    return {};
}

}

namespace RTC
{

MediaFileWriter::MediaFileWriter(FILE* file, uint32_t ssrc,
                                 const std::shared_ptr<RtpMediaFrameSerializer>& serializer,
                                 std::unique_ptr<RtpDepacketizer> depacketizer,
                                 bool liveMode)
    : FileWriter(file)
    , _ssrc(ssrc)
    , _serializer(serializer)
    , _depacketizer(std::move(depacketizer))
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
        if (const auto serializer = RtpMediaFrameSerializer::create(mime)) {
            if (auto depacketizer = RtpDepacketizer::create(mime)) {
                const auto filename = FormatMediaFileName(std::move(outputFolderNameUtf8), mime, ssrc);
                FILE* file = FileOpen(filename, fileOpenError);
                if (file) {
                    return std::make_unique<MediaFileWriter>(file, ssrc, serializer,
                                                             std::move(depacketizer), liveMode);
                }
            }
        }
    }
    return nullptr;
}

void MediaFileWriter::AddPacket(const RtpPacket* packet)
{
    if (packet && _serializer && _depacketizer && packet->GetSsrc() == _ssrc) {
        _serializer->Push(_depacketizer->AddPacket(packet));
    }
}

std::string MediaFileWriter::FormatMediaFileName(std::string_view outputFolderNameUtf8,
                                                 const RTC::RtpCodecMimeType& mime, uint32_t ssrc)
{
    if (!outputFolderNameUtf8.empty()) {
        const auto metrics = GetFileNameMetrics(mime);
        if (!metrics.first.empty() && !metrics.second.empty()) {
            const std::string fileName = metrics.first + std::to_string(ssrc) + "." + metrics.second;
            return std::string(outputFolderNameUtf8.data(), outputFolderNameUtf8.size()) + "/" + fileName;
        }
    }
    return std::string();
}

} // namespace RTC
