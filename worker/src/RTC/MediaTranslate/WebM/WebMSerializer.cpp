#define MS_CLASS "RTC::WebMSerializer"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/MkvBufferedWriter.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "Logger.hpp"

namespace {

using namespace RTC;

template<typename T>
inline constexpr uint64_t ValueToNano(T value) {
    return value * 1000ULL * 1000ULL * 1000ULL;
}

inline bool IsOpus(RtpCodecMimeType::Subtype codec) {
    return RtpCodecMimeType::Subtype::OPUS == codec;
}

inline bool IsOpus(const RtpCodecMimeType& mime) {
    return IsOpus(mime.GetSubtype());
}

}

namespace RTC
{

WebMSerializer::WebMSerializer(uint32_t ssrc, uint32_t clockRate,
                               const RtpCodecMimeType& mime, const char* app)
    : MediaFrameSerializer(ssrc, clockRate, mime)
    , _app(app)
{
    _writers.reserve(2UL);
}

WebMSerializer::~WebMSerializer()
{
    WebMSerializer::RemoveAllSinks();
}

bool WebMSerializer::IsSupported(const RtpCodecMimeType& mimeType)
{
    return nullptr != GetCodecId(mimeType);
}

const char* WebMSerializer::GetCodecId(RtpCodecMimeType::Subtype codec)
{
    // EMBL header for H264 & H265 will be 'matroska' and 'webm' for other codec types
    // https://www.matroska.org/technical/codec_specs.html
    switch (codec) {
        case RtpCodecMimeType::Subtype::VP8:
            return mkvmuxer::Tracks::kVp8CodecId;
        case RtpCodecMimeType::Subtype::VP9:
            return mkvmuxer::Tracks::kVp9CodecId;
        case RtpCodecMimeType::Subtype::H264:
        case RtpCodecMimeType::Subtype::H264_SVC:
            return "V_MPEG4/ISO/AVC"; // matroska
        case RtpCodecMimeType::Subtype::H265:
            return "V_MPEGH/ISO/HEVC";
        case RtpCodecMimeType::Subtype::PCMA:
        case RtpCodecMimeType::Subtype::PCMU:
            return "A_PCM/FLOAT/IEEE";
        default:
            if (IsOpus(codec)) {
                return mkvmuxer::Tracks::kOpusCodecId;
            }
            break;
    }
    return nullptr;
}

const char* WebMSerializer::GetCodecId(const RtpCodecMimeType& mime)
{
    return GetCodecId(mime.GetSubtype());
}

bool WebMSerializer::AddSink(MediaSink* sink)
{
    bool added = false;
    if (sink) {
        added = _writers.end() != _writers.find(sink);
        if (!added) {
            if (auto writer = CreateWriter(sink)) {
                _writers[sink] = std::move(writer);
                added = true;
            }
        }
    }
    return added;
}

bool WebMSerializer::RemoveSink(MediaSink* sink)
{
    if (sink) {
        const auto it = _writers.find(sink);
        if (it != _writers.end()) {
            _writers.erase(it);
            return true;
        }
    }
    return false;
}

void WebMSerializer::RemoveAllSinks()
{
    _writers.clear();
}

bool WebMSerializer::HasSinks() const
{
    return !_writers.empty();
}

size_t WebMSerializer::GetSinksCout() const
{
    return _writers.size();
}

bool WebMSerializer::Push(const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    bool ok = false;
    if (mediaFrame && HasSinks() && IsAccepted(mediaFrame)) {
        const auto mkvTimestamp = UpdateTimeStamp(mediaFrame->GetTimestamp());
        for (auto it = _writers.begin(); it != _writers.end(); ++it) {
            switch (GetMimeType().GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    it->second->SetTrackSettings(_trackNumber, mediaFrame->GetAudioConfig());
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    it->second->SetTrackSettings(_trackNumber, mediaFrame->GetVideoConfig());
                    break;
            }
            ok = it->second->AddFrame(mediaFrame, mkvTimestamp, _trackNumber);
            if (!ok) {
                const auto frameInfo = GetMediaFrameInfoString(mediaFrame, GetSsrc());
                MS_ERROR_STD("unable write frame to MKV data [%s]", frameInfo.c_str());
                break;
            }
        }
    }
    return ok;
}

std::unique_ptr<MkvBufferedWriter> WebMSerializer::CreateWriter(MediaSink* sink) const
{
    if (sink) {
        const auto& mime = GetMimeType();
        const auto ssrc = GetSsrc();
        auto writer = std::make_unique<MkvBufferedWriter>(ssrc, sink, _app);
        if (writer->IsInitialized()) {
            bool ok = false;
            const auto clockRate = GetClockRate();
            switch (mime.GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    ok = writer->AddAudioTrack(_trackNumber, clockRate);
                    if (ok) {
                        ok = writer->SetAudioSampleRate(_trackNumber, clockRate, IsOpus(mime));
                        if (!ok) {
                            MS_ERROR_STD("failed to set intial MKV audio sample rate for %s",
                                         GetStreamInfoString(mime, ssrc).c_str());
                        }
                    }
                    else {
                        MS_ERROR_STD("failed to add MKV audio track for %s",
                                     GetStreamInfoString(mime, ssrc).c_str());
                    }
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    ok = writer->AddVideoTrack(_trackNumber);
                    if (!ok) {
                        MS_ERROR_STD("failed to add MKV video track for %s",
                                     GetStreamInfoString(mime, ssrc).c_str());
                    }
                    break;
            }
            if (ok) {
                ok = writer->SetTrackCodec(_trackNumber, GetCodecId(mime));
                if (!ok) {
                    MS_ERROR_STD("failed to set MKV codec for %s",
                                 GetStreamInfoString(mime, ssrc).c_str());
                    writer.reset();
                }
                return writer;
            }
        }
        else {
            MS_ERROR_STD("failed to initialize MKV writer for %s",
                         GetStreamInfoString(mime, ssrc).c_str());
        }
    }
    return nullptr;
}

bool WebMSerializer::IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const
{
    if (mediaFrame && mediaFrame->GetMimeType() == GetMimeType()) {
        const auto timestamp = mediaFrame->GetTimestamp();
        // special case if both timestamps are zero, for 1st initial frame
        return (0U == timestamp && 0U == _lastTimestamp) || timestamp > _lastTimestamp;
    }
    return false;
}

uint64_t WebMSerializer::UpdateTimeStamp(uint32_t timestamp)
{
    if (timestamp > _lastTimestamp) {
        if (_lastTimestamp) {
            _granule += timestamp - _lastTimestamp;
        }
        _lastTimestamp = timestamp;
    }
    return ValueToNano(_granule) / GetClockRate();
}

} // namespace RTC
