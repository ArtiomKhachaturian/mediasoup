#define MS_CLASS "RTC::RtpWebMDeserializer"
#include "RTC/MediaTranslate/WebM/RtpWebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "Logger.hpp"
#include <array>
#include <mkvparser/mkvreader.h>

namespace {

struct TrackInfo
{
    const RTC::RtpCodecMimeType _mime;
    const std::shared_ptr<RTC::MediaFrameConfig> _config;
    TrackInfo(RTC::RtpCodecMimeType::Type type, RTC::RtpCodecMimeType::Subtype subType,
              std::shared_ptr<RTC::MediaFrameConfig> config);
};

}

namespace RTC
{

class RtpWebMDeserializer::WebMStream
{
public:
    WebMStream(mkvparser::IMkvReader* reader);
    ~WebMStream();
    bool ParseEBMLHeader();
    bool ParseSegment();
    size_t GetTracksCount() const { return _segment ? _segment->GetTracks()->GetTracksCount() : 0UL; }
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const;
    std::shared_ptr<const MediaFrame> ReadNextFrame(size_t trackIndex);
private:
    const mkvparser::Track* GetTrack(size_t trackIndex) const;
    static std::unique_ptr<TrackInfo> GetTrackInfo(const mkvparser::Tracks* tracks, size_t trackIndex);
private:
    mkvparser::IMkvReader* const _reader;
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    mkvparser::Segment* _segment = nullptr;
    //const mkvparser::Cluster* _cluster = nullptr;
    //const mkvparser::BlockEntry* _blockEntry = nullptr;
    //mkvparser::Block* _block = nullptr;
    absl::flat_hash_map<size_t, std::unique_ptr<TrackInfo>> _tracks;
};

RtpWebMDeserializer::RtpWebMDeserializer(mkvparser::IMkvReader* reader)
    : _stream(std::make_unique<WebMStream>(reader))
{
}

RtpWebMDeserializer::~RtpWebMDeserializer()
{
}

bool RtpWebMDeserializer::Update()
{
    if (_ok) {
        _ok = _stream->ParseEBMLHeader() && _stream->ParseSegment();
    }
    return _ok;
}

size_t RtpWebMDeserializer::GetTracksCount() const
{
    return _stream->GetTracksCount();
}

std::optional<RtpCodecMimeType> RtpWebMDeserializer::GetTrackMimeType(size_t trackIndex) const
{
    return _stream->GetTrackMimeType(trackIndex);
}

std::shared_ptr<const MediaFrame> RtpWebMDeserializer::ReadNextFrame(size_t trackIndex)
{
    return _ok ? _stream->ReadNextFrame(trackIndex) : nullptr;
}

bool RtpWebMDeserializer::WebMStream::ParseEBMLHeader()
{
    if (!_ebmlHeader) {
        auto ebmlHeader = std::make_unique<mkvparser::EBMLHeader>();
        long long pos = 0LL;
        if (0L == ebmlHeader->Parse(_reader, pos)) {
            _ebmlHeader = std::move(ebmlHeader);
        }
    }
    return nullptr != _ebmlHeader;
}

bool RtpWebMDeserializer::WebMStream::ParseSegment()
{
    if (!_segment && _ebmlHeader) {
        mkvparser::Segment* segment = nullptr;
        long long pos = 0LL;
        if (0L == mkvparser::Segment::CreateInstance(_reader, pos, segment) &&
            0L == segment->Load()) {
            if (const auto tracks = segment->GetTracks()) {
                if (const auto tracksCount = tracks->GetTracksCount()) {
                    std::swap(_segment, segment);
                    for (size_t i = 0UL; i < tracksCount; ++i) {
                        if (auto trackInfo = GetTrackInfo(tracks, i)) {
                            _tracks[i] = std::move(trackInfo);
                        }
                    }
                }
            }
        }
        delete segment;
    }
    return nullptr != _segment;
}

std::optional<RtpCodecMimeType> RtpWebMDeserializer::WebMStream::GetTrackMimeType(size_t trackIndex) const
{
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        return it->second->_mime;
    }
    return std::nullopt;
}

std::shared_ptr<const MediaFrame> RtpWebMDeserializer::WebMStream::ReadNextFrame(size_t trackIndex)
{
    if (_segment) {
        const auto it = _tracks.find(trackIndex);
        if (it != _tracks.end()) {
            if (const auto track = GetTrack(trackIndex)) {
                
            }
        }
    }
    return nullptr;
}

const mkvparser::Track* RtpWebMDeserializer::WebMStream::GetTrack(size_t trackIndex) const
{
    if (_segment) {
        if (const auto tracks = _segment->GetTracks()) {
            return tracks->GetTrackByIndex(trackIndex);
        }
    }
    return nullptr;
}

std::unique_ptr<TrackInfo> RtpWebMDeserializer::WebMStream::GetTrackInfo(const mkvparser::Tracks* tracks,
                                                                         size_t trackIndex)
{
    if (tracks) {
        if (const auto track = tracks->GetTrackByIndex(trackIndex)) {
            std::optional<RtpCodecMimeType::Type> type;
            switch (track->GetType()) {
                case mkvparser::Track::Type::kVideo:
                    type = RtpCodecMimeType::Type::VIDEO;
                    break;
                case mkvparser::Track::Type::kAudio:
                    type = RtpCodecMimeType::Type::AUDIO;
                    break;
                default:
                    MS_ASSERT(false, "unsupported WebM media track");
                    break;
            }
            if (type.has_value()) {
                std::optional<RtpCodecMimeType::Subtype> subtype;
                for (auto it = RtpCodecMimeType::subtype2String.begin();
                     it != RtpCodecMimeType::subtype2String.end(); ++it) {
                    const auto codecId = RtpWebMSerializer::GetCodecId(it->first);
                    if (codecId && 0 == std::strcmp(track->GetCodecId(), codecId)) {
                        subtype = it->first;
                        break;
                    }
                }
                MS_ASSERT(subtype.has_value(), "unsupported WebM codec type");
                if (subtype.has_value()) {
                    std::shared_ptr<RTC::MediaFrameConfig> config;
                    if (RtpCodecMimeType::Type::VIDEO == type.value()) {
                        auto videoConfig = std::make_shared<VideoFrameConfig>();
                        auto videoTrack = static_cast<const mkvparser::VideoTrack*>(track);
                        videoConfig->SetWidth(videoTrack->GetWidth());
                        videoConfig->SetHeight(videoTrack->GetHeight());
                        videoConfig->SetFrameRate(videoTrack->GetFrameRate());
                        config = std::move(videoConfig);
                    }
                    else {
                        auto audioConfig = std::make_shared<AudioFrameConfig>();
                        auto audioTrack = static_cast<const mkvparser::AudioTrack*>(track);
                        audioConfig->SetChannelCount(audioTrack->GetChannels());
                        audioConfig->SetBitsPerSample(audioTrack->GetBitDepth());
                        config = std::move(audioConfig);
                    }
                    size_t len = 0UL;
                    if (const auto data = track->GetCodecPrivate(len)) {
                        config->SetCodecSpecificData(data, len);
                    }
                    return std::make_unique<TrackInfo>(type.value(), subtype.value(), std::move(config));
                }
            }
        }
    }
    return nullptr;
}

RtpWebMDeserializer::WebMStream::WebMStream(mkvparser::IMkvReader* reader)
    : _reader(reader)
{
    MS_ASSERT(_reader, "MKV reader must not be null");
}

RtpWebMDeserializer::WebMStream::~WebMStream()
{
    delete _segment;
}

} // namespace RTC

namespace {

TrackInfo::TrackInfo(RTC::RtpCodecMimeType::Type type,
                     RTC::RtpCodecMimeType::Subtype subType,
                     std::shared_ptr<RTC::MediaFrameConfig> config)
    : _mime(type, subType)
    , _config(std::move(config))
{
}


}
