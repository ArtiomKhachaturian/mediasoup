#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

// MIME helpers
class RtpStream;
class MediaFrame;

std::string GetMediaFrameInfoString(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                    uint32_t ssrc = 0U);

std::string GetStreamInfoString(const RtpCodecMimeType& mime, uint32_t ssrc = 0U);
std::string GetStreamInfoString(uint32_t ssrc, const RtpStream* stream);

const std::string& MimeTypeToString(RtpCodecMimeType::Type type);
const std::string& MimeTypeToString(const RtpCodecMimeType& mime);

const std::string& MimeSubTypeToString(RtpCodecMimeType::Subtype subtype);
const std::string& MimeSubTypeToString(const RtpCodecMimeType& mime);


} // namespace RTC
