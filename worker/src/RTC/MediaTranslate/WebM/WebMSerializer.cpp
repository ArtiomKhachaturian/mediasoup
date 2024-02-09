#define MS_CLASS "RTC::WebMSerializer"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/MkvBufferedWriter.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "Logger.hpp"

namespace {

template<typename T>
inline constexpr uint64_t ValueToNano(T value) {
    return value * 1000ULL * 1000ULL * 1000ULL;
}

}

namespace RTC
{

class WebMSerializer::Writer : public MkvBufferedWriter
{
public:
    Writer(uint32_t ssrc, MediaSink* sink, const char* app);
    bool AddFrame(const std::shared_ptr<const MediaFrame>& mediaFrame, uint64_t mkvTimestamp);
    void SetTrackSettings(const std::shared_ptr<const AudioFrameConfig>& config);
    void SetTrackSettings(const std::shared_ptr<const VideoFrameConfig>& config);
    void SetSingleTrackNumber(uint64_t trackNumber) { _singleTrackNumber = trackNumber; }
private:
    uint64_t _singleTrackNumber = 0ULL;
};

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
                    it->second->SetTrackSettings(mediaFrame->GetAudioConfig());
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    it->second->SetTrackSettings(mediaFrame->GetVideoConfig());
                    break;
            }
            ok = it->second->AddFrame(mediaFrame, mkvTimestamp);
            if (!ok) {
                const auto frameInfo = GetMediaFrameInfoString(mediaFrame, GetSsrc());
                MS_ERROR_STD("unable write frame to MKV data [%s]", frameInfo.c_str());
                break;
            }
        }
    }
    return ok;
}

std::unique_ptr<WebMSerializer::Writer> WebMSerializer::CreateWriter(MediaSink* sink) const
{
    if (sink) {
        const auto& mime = GetMimeType();
        const auto ssrc = GetSsrc();
        auto writer = std::make_unique<Writer>(ssrc, sink, _app);
        if (writer->IsInitialized()) {
            const auto clockRate = GetClockRate();
            uint64_t trackNumber = 0ULL;
            bool ok = false;
            switch (mime.GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    trackNumber = writer->AddAudioTrack(clockRate);
                    if (trackNumber) {
                        const auto opusAudio = RtpCodecMimeType::Subtype::OPUS == mime.GetSubtype();
                        ok = writer->SetAudioSampleRate(trackNumber, clockRate, opusAudio);
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
                    trackNumber = writer->AddVideoTrack();
                    if (!ok) {
                        MS_ERROR_STD("failed to add MKV video track for %s",
                                     GetStreamInfoString(mime, ssrc).c_str());
                    }
                    break;
            }
            if (ok) {
                ok = writer->SetTrackCodec(trackNumber, mime);
                if (!ok) {
                    MS_ERROR_STD("failed to set MKV codec for %s",
                                 GetStreamInfoString(mime, ssrc).c_str());
                    writer.reset();
                }
                else {
                    writer->SetSingleTrackNumber(trackNumber);
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

WebMSerializer::Writer::Writer(uint32_t ssrc, MediaSink* sink, const char* app)
    : MkvBufferedWriter(ssrc, sink, app)
{
}

bool WebMSerializer::Writer::AddFrame(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                      uint64_t mkvTimestamp)
{
    return MkvBufferedWriter::AddFrame(_singleTrackNumber, mediaFrame, mkvTimestamp);
}

void WebMSerializer::Writer::SetTrackSettings(const std::shared_ptr<const AudioFrameConfig>& config)
{
    MkvBufferedWriter::SetTrackSettings(_singleTrackNumber, config);
}

void WebMSerializer::Writer::SetTrackSettings(const std::shared_ptr<const VideoFrameConfig>& config)
{
    MkvBufferedWriter::SetTrackSettings(_singleTrackNumber, config);
}

} // namespace RTC
