#pragma once

#include "RTC/MediaTranslate/FileWriter.hpp"
#include "RTC/RtpPacketsCollector.hpp"

namespace RTC
{

class RtpCodecMimeType;
class RtpDepacketizer;
class RtpMediaFrameSerializer;

class MediaFileWriter : public FileWriter, public RtpPacketsCollector
{
public:
    MediaFileWriter(FILE* file, uint32_t ssrc,
                    std::unique_ptr<RtpMediaFrameSerializer> serializer,
                    const std::shared_ptr<RtpDepacketizer>& depacketizer,
                    bool liveMode = false);
    ~MediaFileWriter() final;
    void SetTargetPacketsCollector(RtpPacketsCollector* targetPacketsCollector);
    // impl. of RtpPacketsCollector
    void AddPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet) final;
    static std::shared_ptr<MediaFileWriter> Create(std::string_view outputFolderNameUtf8,
                                                   const RTC::RtpCodecMimeType& mime,
                                                   uint32_t ssrc, bool liveMode = false,
                                                   int* fileOpenError = nullptr);
private:
    static std::string FormatMediaFileName(const RTC::RtpCodecMimeType& mime, uint32_t ssrc);
    static std::string FormatMediaFileName(std::string_view outputFolderNameUtf8,
                                           const RTC::RtpCodecMimeType& mime, uint32_t ssrc);
private:
    const uint32_t _ssrc;
    const std::unique_ptr<RtpMediaFrameSerializer> _serializer;
    const std::shared_ptr<RtpDepacketizer> _depacketizer;
    RtpPacketsCollector* _targetPacketsCollector = nullptr;
};

} // namespace RTC
