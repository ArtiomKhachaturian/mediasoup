#define MS_CLASS "RTC::WebMDeserializer"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/MkvReader.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "api/units/timestamp.h"
#include "Logger.hpp"
#include "DepLibUV.hpp"
#include <array>

namespace {

template<typename T>
inline constexpr T ValueFromNano(unsigned long long nano) {
    return static_cast<T>(nano / 1000 / 1000 / 1000);
}

}

namespace RTC
{

class WebMDeserializer::WebMStream
{
public:
    WebMStream(mkvparser::IMkvReader* reader, bool loopback);
    ~WebMStream();
    // both parse methods returns true if EMBL header or MKV segmen is already parsed
    bool ParseEBMLHeader();
    bool ParseSegment();
    size_t GetTracksCount() const { return _segment ? _segment->GetTracks()->GetTracksCount() : 0UL; }
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const;
    std::vector<std::shared_ptr<const MediaFrame>> ReadNextFrames(size_t trackIndex);
    void SetClockRate(size_t trackIndex, uint32_t clockRate);
    void SetInitialTimestamp(size_t trackIndex, uint32_t initialTimestamp);
private:
    mkvparser::IMkvReader* const _reader;
    const bool _loopback;
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    mkvparser::Segment* _segment = nullptr;
    absl::flat_hash_map<size_t, std::unique_ptr<TrackInfo>> _tracks;
};

class WebMDeserializer::TrackInfo
{
public:
    TrackInfo(RtpCodecMimeType::Type type,
              RtpCodecMimeType::Subtype subType,
              std::shared_ptr<MediaFrameConfig> config,
              const mkvparser::Track* track);
    const RtpCodecMimeType& GetMime() const { return _mime; }
    const std::shared_ptr<MediaFrameConfig>& GetConfig() const { return _config; }
    const mkvparser::Block* GetNextBlock(bool loopback);
    uint32_t GetCurrentTimestamp() const;
    uint32_t GetCurrentTimestamp(const mkvparser::Block* block) const;
    void SetClockRate(uint32_t clockRate);
    uint32_t GetClockRate() const { return _clockRate; }
    void SetInitialTimestamp(uint32_t initialTimestamp);
    uint32_t GetInitialTimestamp() const { return _initialTimestamp; }
    static std::unique_ptr<TrackInfo> Create(const mkvparser::Tracks* tracks, size_t trackIndex);
private:
    const mkvparser::Cluster* GetCurrentCluster() const;
private:
    const RtpCodecMimeType _mime;
    const std::shared_ptr<MediaFrameConfig> _config;
    const mkvparser::Track* const _track;
    const mkvparser::BlockEntry* _currentBlockEntry = nullptr;
    uint32_t _clockRate = 0U;
    uint32_t _initialTimestamp = 0U;
};

WebMDeserializer::WebMDeserializer(std::unique_ptr<MkvReader> reader, bool loopback)
    : _reader(std::move(reader))
    , _stream(std::make_unique<WebMStream>(_reader.get(), loopback))
{
    MS_ASSERT(_reader, "MKV reader must not be null");
}

WebMDeserializer::~WebMDeserializer()
{
}

bool WebMDeserializer::AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (_reader->AddBuffer(buffer)) {
        return _stream->ParseEBMLHeader() && _stream->ParseSegment();
    }
    return false;
}

size_t WebMDeserializer::GetTracksCount() const
{
    return _stream->GetTracksCount();
}

std::optional<RtpCodecMimeType> WebMDeserializer::GetTrackMimeType(size_t trackIndex) const
{
    return _stream->GetTrackMimeType(trackIndex);
}

std::vector<std::shared_ptr<const MediaFrame>> WebMDeserializer::ReadNextFrames(size_t trackIndex)
{
    return _stream->ReadNextFrames(trackIndex);
}

void WebMDeserializer::SetClockRate(size_t trackIndex, uint32_t clockRate)
{
    MediaFrameDeserializer::SetClockRate(trackIndex, clockRate);
    _stream->SetClockRate(trackIndex, clockRate);
}

void WebMDeserializer::SetInitialTimestamp(size_t trackIndex, uint32_t initialTimestamp)
{
    MediaFrameDeserializer::SetInitialTimestamp(trackIndex, initialTimestamp);
    _stream->SetInitialTimestamp(trackIndex, initialTimestamp);
}

WebMDeserializer::WebMStream::WebMStream(mkvparser::IMkvReader* reader, bool loopback)
    : _reader(reader)
    , _loopback(loopback)
{
}

WebMDeserializer::WebMStream::~WebMStream()
{
    delete _segment;
}

bool WebMDeserializer::WebMStream::ParseEBMLHeader()
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

bool WebMDeserializer::WebMStream::ParseSegment()
{
    if (!_segment && _ebmlHeader) {
        mkvparser::Segment* segment = nullptr;
        long long pos = 0LL;
        if (0L == mkvparser::Segment::CreateInstance(_reader, pos, segment)) {
            const auto loadResult = segment->Load();
            if (0 == loadResult || mkvparser::E_BUFFER_NOT_FULL == loadResult) {
                if (const auto tracks = segment->GetTracks()) {
                    if (const auto tracksCount = tracks->GetTracksCount()) {
                        std::swap(_segment, segment);
                        for (size_t i = 0UL; i < tracksCount; ++i) {
                            if (auto trackInfo = TrackInfo::Create(tracks, i)) {
                                _tracks[i] = std::move(trackInfo);
                            }
                        }
                    }
                }
            }
        }
        delete segment;
    }
    return nullptr != _segment;
}

std::optional<RtpCodecMimeType> WebMDeserializer::WebMStream::GetTrackMimeType(size_t trackIndex) const
{
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        return it->second->GetMime();
    }
    return std::nullopt;
}

std::vector<std::shared_ptr<const MediaFrame>> WebMDeserializer::WebMStream::ReadNextFrames(size_t trackIndex)
{
    if (_segment) {
        const auto it = _tracks.find(trackIndex);
        if (it != _tracks.end()) {
            if (const auto block = it->second->GetNextBlock(_loopback)) {
                const auto framesCount = block->GetFrameCount();
                if (framesCount > 0) {
                    std::vector<std::shared_ptr<const MediaFrame>> frames;
                    frames.reserve(static_cast<size_t>(framesCount));
                    for (int i = 0; i < framesCount; ++i) {
                        const auto& webmFrame = block->GetFrame(i);
                        std::vector<uint8_t> buffer(webmFrame.len);
                        if (0 == webmFrame.Read(_reader, buffer.data())) {
                            auto mediaFrame = std::make_shared<MediaFrame>(it->second->GetMime());
                            if (mediaFrame->AddPayload(std::move(buffer))) {
                                mediaFrame->SetKeyFrame(block->IsKey());
                                mediaFrame->SetMediaConfig(it->second->GetConfig());
                                mediaFrame->SeTimestamp(it->second->GetCurrentTimestamp(block));
                                frames.push_back(std::move(mediaFrame));
                            }
                        }
                    }
                    return frames;
                }
            }
        }
    }
    return {};
}

void WebMDeserializer::WebMStream::SetClockRate(size_t trackIndex, uint32_t clockRate)
{
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        it->second->SetClockRate(clockRate);
    }
}

void WebMDeserializer::WebMStream::SetInitialTimestamp(size_t trackIndex,
                                                       uint32_t initialTimestamp)
{
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        it->second->SetInitialTimestamp(initialTimestamp);
    }
}

WebMDeserializer::TrackInfo::TrackInfo(RtpCodecMimeType::Type type,
                                       RtpCodecMimeType::Subtype subType,
                                       std::shared_ptr<MediaFrameConfig> config,
                                       const mkvparser::Track* track)
    : _mime(type, subType)
    , _config(std::move(config))
    , _track(track)
{
    if (RtpCodecMimeType::Type::AUDIO == type) {
        SetClockRate(static_cast<const mkvparser::AudioTrack*>(track)->GetSamplingRate());
    }
}

const mkvparser::Block* WebMDeserializer::TrackInfo::GetNextBlock(bool loopback)
{
    const mkvparser::BlockEntry* blockEntry = nullptr;
    if (!_currentBlockEntry) {
        //_initialTimestamp = webrtc::Timestamp::ms(DepLibUV::GetTimeMs()).ms<uint32_t>();
        //_initialTimestamp = static_cast<uint32_t>(DepLibUV::GetTimeMs());
        _track->GetFirst(blockEntry);
    }
    else {
        _track->GetNext(_currentBlockEntry, blockEntry);
    }
    if (blockEntry) {
        if (!blockEntry->EOS()) {
            if (const auto block = blockEntry->GetBlock()) {
                _currentBlockEntry = blockEntry;
                return block;
            }
        }
        else if (loopback) {
            _currentBlockEntry = nullptr;
            return GetNextBlock(false);
        }
    }
    return nullptr;
}

uint32_t WebMDeserializer::TrackInfo::GetCurrentTimestamp() const
{
    if (GetClockRate() && _currentBlockEntry) {
        return GetCurrentTimestamp(_currentBlockEntry->GetBlock());
    }
    return 0U;
}

uint32_t WebMDeserializer::TrackInfo::GetCurrentTimestamp(const mkvparser::Block* block) const
{
    if (block && GetClockRate()) {
        if (const auto cluster = GetCurrentCluster()) {
            const auto granule = ValueFromNano<uint32_t>(block->GetTime(cluster) * GetClockRate());
            return GetInitialTimestamp() + granule;
        }
    }
    return 0U;
}

void WebMDeserializer::TrackInfo::SetClockRate(uint32_t clockRate)
{
    MS_ASSERT(clockRate > 0U, "clock rate must be greater than zero");
    _clockRate = clockRate;
}

void WebMDeserializer::TrackInfo::SetInitialTimestamp(uint32_t initialTimestamp)
{
    _initialTimestamp = initialTimestamp;
}

const mkvparser::Cluster* WebMDeserializer::TrackInfo::GetCurrentCluster() const
{
    return _currentBlockEntry ? _currentBlockEntry->GetCluster() : nullptr;
}

std::unique_ptr<WebMDeserializer::TrackInfo> WebMDeserializer::TrackInfo::
    Create(const mkvparser::Tracks* tracks, size_t trackIndex)
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
                    MS_WARN_TAG(rtp, "Unsupported WebM media track type: %ld", track->GetType());
                    break;
            }
            if (type.has_value()) {
                std::optional<RtpCodecMimeType::Subtype> subtype;
                for (auto it = RtpCodecMimeType::subtype2String.begin();
                     it != RtpCodecMimeType::subtype2String.end(); ++it) {
                    const auto codecId = WebMSerializer::GetCodecId(it->first);
                    if (codecId && 0 == std::strcmp(track->GetCodecId(), codecId)) {
                        subtype = it->first;
                        break;
                    }
                }
                if (subtype.has_value()) {
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
                        return std::make_unique<TrackInfo>(type.value(), subtype.value(),
                                                           std::move(config), track);
                    }
                }
                else {
                    MS_WARN_TAG(rtp, "Unsupported WebM codec type: %s", track->GetCodecId());
                }
            }
        }
    }
    return nullptr;
}

} // namespace RTC
