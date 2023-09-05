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
                    const std::shared_ptr<RtpMediaFrameSerializer>& serializer,
                    std::unique_ptr<RtpDepacketizer> depacketizer,
                    bool liveMode = false);
    ~MediaFileWriter() final;
    // impl. of RtpPacketsCollector
    void AddPacket(const RtpPacket* packet) final;
    static std::shared_ptr<MediaFileWriter> Create(std::string_view outputFolderNameUtf8,
                                                   const RTC::RtpCodecMimeType& mime,
                                                   uint32_t ssrc, uint32_t sampleRate,
                                                   bool liveMode = false,
                                                   int* fileOpenError = nullptr);
private:
    static std::string FormatMediaFileName(std::string_view outputFolderNameUtf8,
                                           const RTC::RtpCodecMimeType& mime, uint32_t ssrc);
private:
    const uint32_t _ssrc;
    const std::shared_ptr<RtpMediaFrameSerializer> _serializer;
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
};

} // namespace RTC
