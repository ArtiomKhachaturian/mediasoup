#define MS_CLASS "RTC::WebMSerializer"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/MkvBufferedWriter.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/MediaFrameWriter.hpp"
#include "Logger.hpp"

namespace RTC
{

class WebMSerializer::Writer : public MediaFrameWriter
{
public:
    Writer(const RtpCodecMimeType& mime, MediaSink* sink,
           const std::shared_ptr<BufferAllocator>& allocator);
    bool IsInitialized() const { return 0U != _trackNumber; }
    // impl. of MediaFrameWriter
    bool Write(const std::shared_ptr<const MediaFrame>& mediaFrame,
               const webrtc::TimeDelta& offset) final;
    void SetConfig(const std::shared_ptr<const AudioFrameConfig>& config) final;
    void SetConfig(const std::shared_ptr<const VideoFrameConfig>& config) final;
private:
    MkvBufferedWriter _impl;
    uint64_t _trackNumber = 0U;
};

WebMSerializer::WebMSerializer(const RtpCodecMimeType& mime,
                               const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<MediaFrameSerializer>(allocator, mime)
{
    MS_ASSERT(WebMCodecs::IsSupported(mime), "WebM not available for this MIME %s", mime.ToString().c_str());
}
std::unique_ptr<MediaFrameWriter> WebMSerializer::CreateWriter(MediaSink* sink)
{
    if (sink) {
        auto writer = std::make_unique<Writer>(GetMimeType(), sink, GetAllocator());
        if (writer->IsInitialized()) {
            return writer;
        }
    }
    return nullptr;
}

WebMSerializer::Writer::Writer(const RtpCodecMimeType& mime, MediaSink* sink,
                               const std::shared_ptr<BufferAllocator>& allocator)
    : _impl(sink, GetAgentName(), allocator)
{
    if (_impl.IsInitialized()) {
        uint64_t trackNumber = 0ULL;
        switch (mime.GetType()) {
            case RtpCodecMimeType::Type::AUDIO:
                trackNumber = _impl.AddAudioTrack();
                break;
            case RtpCodecMimeType::Type::VIDEO:
                trackNumber = _impl.AddVideoTrack();
                break;
            default:
                break;
        }
        if (trackNumber) {
            if (_impl.SetTrackCodec(trackNumber, mime)) {
                _trackNumber = trackNumber;
            }
            else {
                MS_ERROR_STD("failed to set MKV codec for %s", GetStreamInfoString(mime).c_str());
            }
        }
        else {
            MS_ERROR_STD("failed to add MKV track for %s", GetStreamInfoString(mime).c_str());
        }
    }
    else {
        MS_ERROR_STD("failed to initialize MKV writer for %s", GetStreamInfoString(mime).c_str());
    }
}

bool WebMSerializer::Writer::Write(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                   const webrtc::TimeDelta& offset)
{
    return _impl.AddFrame(_trackNumber, mediaFrame, offset.ns<uint64_t>());
}

void WebMSerializer::Writer::SetConfig(const std::shared_ptr<const AudioFrameConfig>& config)
{
    _impl.SetTrackSettings(_trackNumber, config);
}

void WebMSerializer::Writer::SetConfig(const std::shared_ptr<const VideoFrameConfig>& config)
{
    _impl.SetTrackSettings(_trackNumber, config);
}

} // namespace RTC
