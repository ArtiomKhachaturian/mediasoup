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

namespace {

template<typename T>
inline constexpr T ValueFromNano(unsigned long long nano) {
    return static_cast<T>(nano / 1000 / 1000 / 1000);
}

}

namespace RTC
{

class WebMDeserializer::TrackInfo
{
public:
    TrackInfo(RtpCodecMimeType::Type type,
              RtpCodecMimeType::Subtype subType,
              std::shared_ptr<MediaFrameConfig> config,
              mkvparser::Segment* segment,
              long long trackNum);
    const RtpCodecMimeType& GetMime() const { return _mime; }
    const std::shared_ptr<MediaFrameConfig>& GetConfig() const { return _config; }
    bool IsEOS() const { return _cluster == nullptr || _cluster->EOS(); }
    MediaFrameDeserializeResult Advance();
    void Reset();
    void SetClockRate(uint32_t clockRate);
    void SetInitialTimestamp(uint32_t timestamp);
    MediaFrameDeserializeResult ReadFrames(std::vector<std::shared_ptr<const MediaFrame>>& output);
    static std::unique_ptr<TrackInfo> Create(const mkvparser::Tracks* tracks,
                                             mkvparser::Segment* segment,
                                             unsigned long trackIndex);
private:
    const mkvparser::Block* Block() const;
    std::optional<uint32_t> BlockTime() const;
    uint32_t GetClockRate() const { return _clockRate; }
    uint32_t GetInitialTimestamp() const { return _initialTimestamp; }
private:
    const RtpCodecMimeType _mime;
    const std::shared_ptr<MediaFrameConfig> _config;
    mkvparser::Segment* const _segment;
    const long long _trackNum;
    const mkvparser::Cluster* _cluster = nullptr;
    const mkvparser::BlockEntry* _blockEntry = nullptr;
    long _blockEntryIndex = 0L;
    uint32_t _clockRate = 0U;
    uint32_t _initialTimestamp = 0U;
    uint32_t _latestTimestamp = 0U;
};

WebMDeserializer::WebMDeserializer(std::unique_ptr<MkvReader> reader, bool loopback)
    : _reader(std::move(reader))
    , _loopback(loopback)
{
    MS_ASSERT(_reader, "MKV reader must not be null");
}

WebMDeserializer::~WebMDeserializer()
{
    delete _segment;
}

MediaFrameDeserializeResult WebMDeserializer::AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    auto result = _reader->AddBuffer(buffer);
    if (MediaFrameDeserializeResult::Success == result) {
        result = ParseEBMLHeader();
        if (MediaFrameDeserializeResult::Success == result) {
            result = ParseSegment();
        }
    }
    return result;
}

MediaFrameDeserializeResult WebMDeserializer::ReadNextFrames(size_t trackIndex,
                                                             std::vector<std::shared_ptr<const MediaFrame>>& output)
{
    MediaFrameDeserializeResult result = MediaFrameDeserializeResult::ParseError;
    if (_segment) {
        const auto it = _tracks.find(trackIndex);
        if (it != _tracks.end()) {
            if (it->second->IsEOS()) {
                result = MediaFrameDeserializeResult::NeedMoreData;
            }
            else {
                result = it->second->ReadFrames(output);
            }
            // lookup the next blocks if any
            if (MediaFrameDeserializeResult::Success == result ||
                MediaFrameDeserializeResult::NeedMoreData == result) {
                const auto advanceResult = it->second->Advance();
                switch (advanceResult) {
                    case MediaFrameDeserializeResult::ParseError:
                    case MediaFrameDeserializeResult::OutOfMemory:
                    case MediaFrameDeserializeResult::InvalidArg:
                        result = advanceResult;
                        break;
                    case MediaFrameDeserializeResult::NeedMoreData:
                        if (_loopback) {
                            long long total = 0LL, available = 0LL;
                            // check if not live stream
                            if (0L == _reader->Length(&total, &available) && total > 0LL) {
                                it->second->Reset();
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        else {
            result = MediaFrameDeserializeResult::InvalidArg;
        }
    }
    return result;
}

size_t WebMDeserializer::GetTracksCount() const
{
    return _tracks.size();
}

std::optional<RtpCodecMimeType> WebMDeserializer::GetTrackMimeType(size_t trackIndex) const
{
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        return it->second->GetMime();
    }
    return std::nullopt;
}

void WebMDeserializer::SetClockRate(size_t trackIndex, uint32_t clockRate)
{
    MediaFrameDeserializer::SetClockRate(trackIndex, clockRate);
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        it->second->SetClockRate(clockRate);
    }
}

void WebMDeserializer::SetInitialTimestamp(uint32_t timestamp)
{
    MediaFrameDeserializer::SetInitialTimestamp(timestamp);
    if (timestamp != _initialTimestamp) {
        _initialTimestamp = timestamp;
        for (auto it = _tracks.begin(); it != _tracks.end(); ++it) {
            it->second->SetInitialTimestamp(_initialTimestamp);
        }
    }
}

MediaFrameDeserializeResult WebMDeserializer::ParseEBMLHeader()
{
    auto result = MediaFrameDeserializeResult::Success;
    if (!_ebmlHeader) {
        auto ebmlHeader = std::make_unique<mkvparser::EBMLHeader>();
        long long pos = 0LL;
        result = ToResult(ebmlHeader->Parse(_reader.get(), pos));
        if (MediaFrameDeserializeResult::Success == result) {
            _ebmlHeader = std::move(ebmlHeader);
        }
    }
    return result;
}

MediaFrameDeserializeResult WebMDeserializer::ParseSegment()
{
    auto result = MediaFrameDeserializeResult::Success;
    if (!_segment) {
        mkvparser::Segment* segment = nullptr;
        long long pos = 0LL;
        result = ToResult(mkvparser::Segment::CreateInstance(_reader.get(), pos, segment));
        if (MediaFrameDeserializeResult::Success == result) {
            result = ToResult(segment->Load());
            if (MediaFrameDeserializeResult::Success == result ||
                MediaFrameDeserializeResult::NeedMoreData == result) {
                if (const auto tracks = segment->GetTracks()) {
                    if (const auto tracksCount = tracks->GetTracksCount()) {
                        std::swap(_segment, segment);
                        for (unsigned long i = 0UL; i < tracksCount; ++i) {
                            if (auto trackInfo = TrackInfo::Create(tracks, _segment, i)) {
                                _tracks[i] = std::move(trackInfo);
                            }
                        }
                    }
                }
            }
        }
        delete segment;
    }
    return result;
}

template<typename mkvResult>
MediaFrameDeserializeResult WebMDeserializer::ToResult(mkvResult result)
{
    switch (result) {
        case 0:
            break;
        case mkvparser::E_BUFFER_NOT_FULL:
        case 1: // // no more clusters
            return MediaFrameDeserializeResult::NeedMoreData;
        default:
            return MediaFrameDeserializeResult::ParseError;
    }
    return MediaFrameDeserializeResult::Success;
}

WebMDeserializer::TrackInfo::TrackInfo(RtpCodecMimeType::Type type,
                                       RtpCodecMimeType::Subtype subType,
                                       std::shared_ptr<MediaFrameConfig> config,
                                       mkvparser::Segment* segment,
                                       long long trackNum)
    : _mime(type, subType)
    , _config(std::move(config))
    , _segment(segment)
    , _trackNum(trackNum)
{
    Reset();
}

MediaFrameDeserializeResult WebMDeserializer::TrackInfo::Advance()
{
    if (_cluster) {
        for (;;) {
            long res = _cluster->GetEntry(_blockEntryIndex, _blockEntry);
            MS_DEBUG_DEV("Cluster::GetEntry returned %ld", res);
            long long pos;
            long len;
            if (res < 0) {
                // Need to parse this cluster some more
                if (mkvparser::E_BUFFER_NOT_FULL == res) {
                    res = _cluster->Parse(pos, len);
                    MS_DEBUG_DEV("Cluster::Parse returned %ld", res);
                }
                if (res < 0) {
                    // I/O error
                    MS_ERROR("Cluster::Parse returned result %ld", res);
                    _cluster = NULL;
                    return ToResult(res);
                }
                continue;
            } else if (res == 0) {
                // We're done with this cluster
                const mkvparser::Cluster* nextCluster = nullptr;
                res = _segment->ParseNext(_cluster, nextCluster, pos, len);
                MS_DEBUG_DEV("Segment::ParseNext returned %ld", res);
                if (res != 0) {
                    // EOF or error
                    if (res < 0) {
                        _cluster = NULL;
                    }
                    return ToResult(res);
                }
                MS_ASSERT(nullptr != nextCluster, "next WebM cluster must not be null");
                MS_ASSERT(!nextCluster->EOS(), "next WebM cluster is end of the stream");
                _cluster = nextCluster;
                res = _cluster->Parse(pos, len);
                MS_DEBUG_DEV("Cluster::Parse (2) returned %ld", res);
                if (res < 0) {
                    // I/O error
                    if (mkvparser::E_BUFFER_NOT_FULL != res) {
                        MS_ERROR("Cluster::Parse (2) returned result %ld", res);
                    }
                    _cluster = NULL;
                    return ToResult(res);
                }
                _blockEntryIndex = 0;
                continue;
            }
            MS_ASSERT(nullptr != _blockEntry, "block entry must not be null");
            MS_ASSERT(nullptr != _blockEntry->GetBlock(), "block must not be null");
            ++_blockEntryIndex;
            if (_blockEntry->GetBlock()->GetTrackNumber() == _trackNum) {
                break;
            }
        }
        return MediaFrameDeserializeResult::Success;
    }
    return MediaFrameDeserializeResult::ParseError;
}

void WebMDeserializer::TrackInfo::Reset()
{
    _cluster = _segment->GetFirst();
    _blockEntry = NULL;
    _blockEntryIndex = 0L;
    _initialTimestamp = _latestTimestamp;
    do {
        Advance();
    } while (!IsEOS() && Block()->GetTrackNumber() != _trackNum);
}

void WebMDeserializer::TrackInfo::SetClockRate(uint32_t clockRate)
{
    MS_ASSERT(clockRate > 0U, "clock rate must be greater than zero");
    _clockRate = clockRate;
}

void WebMDeserializer::TrackInfo::SetInitialTimestamp(uint32_t timestamp)
{
    if (timestamp != _initialTimestamp) {
        _latestTimestamp = _initialTimestamp = timestamp;
    }
}

MediaFrameDeserializeResult WebMDeserializer::TrackInfo::
    ReadFrames(std::vector<std::shared_ptr<const MediaFrame>>& output)
{
    MediaFrameDeserializeResult result = MediaFrameDeserializeResult::ParseError;
    if (const auto block = Block()) {
        const auto ts = BlockTime();
        output.clear();
        output.reserve(static_cast<size_t>(block->GetFrameCount()));
        for (int i = 0; i < block->GetFrameCount(); ++i) {
            const auto& frame = block->GetFrame(i);
            std::vector<uint8_t> buffer(frame.len);
            result = ToResult(frame.Read(_segment->m_pReader, buffer.data()));
            if (MediaFrameDeserializeResult::Success == result) {
                auto mediaFrame = std::make_shared<MediaFrame>(GetMime());
                if (mediaFrame->AddPayload(std::move(buffer))) {
                    mediaFrame->SetKeyFrame(block->IsKey());
                    mediaFrame->SetMediaConfig(GetConfig());
                    if (ts.has_value()) {
                        mediaFrame->SeTimestamp(ts.value());
                    }
                    output.push_back(std::move(mediaFrame));
                }
            }
            else {
                break;
            }
        }
        if (!output.empty() && ts.has_value()) {
            _latestTimestamp = ts.value();
        }
    }
    return result;
}

const mkvparser::Block* WebMDeserializer::TrackInfo::Block() const
{
    MS_ASSERT(!IsEOS(), "end of stream");
    return _blockEntry->GetBlock();
}

std::optional<uint32_t> WebMDeserializer::TrackInfo::BlockTime() const
{
    if (_blockEntry && _cluster) {
        const auto granule = ValueFromNano<uint32_t>(Block()->GetTime(_cluster) * GetClockRate());
        return GetInitialTimestamp() + granule;
    }
    return std::nullopt;
}

std::unique_ptr<WebMDeserializer::TrackInfo> WebMDeserializer::TrackInfo::
    Create(const mkvparser::Tracks* tracks, mkvparser::Segment* segment, unsigned long trackIndex)
{
    if (tracks && segment) {
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
                        auto trackInfo = std::make_unique<TrackInfo>(type.value(),
                                                                     subtype.value(),
                                                                     std::move(config),
                                                                     segment,
                                                                     track->GetNumber());
                        if (RtpCodecMimeType::Type::AUDIO == type.value()) {
                            trackInfo->SetClockRate(static_cast<const mkvparser::AudioTrack*>(track)->GetSamplingRate());
                        }
                        return trackInfo;
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
