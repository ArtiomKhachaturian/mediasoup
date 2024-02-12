#define MS_CLASS "RTC::RtpPacketsPlayer"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpMemoryBufferPacket.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpPacketsInfoProvider.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"
#include "absl/container/flat_hash_set.h"
#include <variant>
#include <queue>

namespace {

using namespace RTC;

class MediaInfo
{
public:
    virtual ~MediaInfo() = default;
    uint32_t GetSsrc() const { return _ssrc; }
    RtpPacketsPlayerCallback* GetPacketsCollector() const { return _packetsCollector; }
    const RtpPacketsInfoProvider* GetPacketsInfoProvider() const { return _packetsInfoProvider; }
    void SetResult(MediaFrameDeserializeResult result, const char* operationName = "");
protected:
    MediaInfo(uint32_t ssrc,
              RtpPacketsPlayerCallback* packetsCollector,
              const RtpPacketsInfoProvider* packetsInfoProvider);
    void ResetResult() { _lastResult.store(MediaFrameDeserializeResult::Success); }
    //uint16_t GetLastOriginalRtpSeqNumber() const;
    //uint32_t GetLastOriginalRtpTimestamp() const;
    uint32_t GetClockRate() const;
    virtual std::string GetDescription() const { return ""; }
private:
    const uint32_t _ssrc;
    RtpPacketsPlayerCallback* const _packetsCollector;
    const RtpPacketsInfoProvider* const _packetsInfoProvider;
    std::atomic<MediaFrameDeserializeResult> _lastResult = MediaFrameDeserializeResult::Success;
};

enum class PlayTaskType {
    Started,
    Frame,
    Finished
};

class PlayTask
{
public:
    PlayTask(bool started);
    PlayTask(const std::shared_ptr<const MediaFrame>& frame);
    PlayTask(const PlayTask&) = default;
    PlayTask(PlayTask&&) = default;
    PlayTaskType GetType() const;
    std::shared_ptr<const MediaFrame> GetFrame() const;
    PlayTask& operator = (const PlayTask&) = default;
    PlayTask& operator = (PlayTask&&) = default;
private:
    std::variant<bool, std::shared_ptr<const MediaFrame>> _data;
};

}

namespace RTC
{

/*class RtpPacketsPlayer::TrackPlayer : public MediaInfo, public MediaTimerCallback
{
public:
    TrackPlayer(uint32_t ssrc,
                RtpPacketsPlayerCallback* packetsCollector,
                const RtpPacketsInfoProvider* packetsInfoProvider,
                size_t trackIndex, MediaTimer* timer,
                std::unique_ptr<RtpPacketizer> packetizer);
    ~TrackPlayer() final;
    void SetTimerEventId(uint64_t timerEventId) { _timerEventId = timerEventId; }
    uint64_t GetTimerEventId() const { return _timerEventId.load(); }
    size_t GetTrackIndex() const { return _trackIndex; }
    void Enque(PlayTask task);
private:
    RtpPacket* CreatePacket(const std::shared_ptr<const MediaFrame>& frame);
    // impl. of LoopTimerCallback
    void OnEvent() final;
private:
    const size_t _trackIndex;
    MediaTimer* const _timer;
    const std::unique_ptr<RtpPacketizer> _packetizer;
    std::atomic<uint64_t> _timerEventId = 0ULL;
    uint16_t _sequenceNumber = 0U;
    uint32_t _initialRtpTimestamp = 0U;
    uint32_t _playoutOffset = 0U;
    ProtectedObj<std::queue<PlayTask>> _tasks;
};

class RtpPacketsPlayer::Stream : public MediaInfo
{
    using PlayersList = absl::flat_hash_set<std::shared_ptr<TrackPlayer>>;
public:
    Stream(uint32_t ssrc,
           const RtpCodecMimeType& mime,
           RtpPacketsPlayerCallback* packetsCollector,
           const RtpPacketsInfoProvider* packetsInfoProvider,
           MediaTimer* timer);
    ~Stream();
    void StartMediaWriting();
    void WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer);
    void EndMediaWriting();
    // override of MediaInfo
    std::string GetDescription() const { return GetStreamInfoString(_mime, GetSsrc()); }
private:
    void SetDeserializer(std::shared_ptr<MediaFrameDeserializer> deserializer);
    void ClearTrackPlayers(bool destroy);
    bool FetchMediaInfo(const std::shared_ptr<MediaFrameDeserializer>& deserializer);
    void DeserializeMediaFrames(const std::shared_ptr<MediaFrameDeserializer>& deserializer);
private:
    const RtpCodecMimeType _mime;
    MediaTimer* const _timer;
    std::shared_ptr<MediaFrameDeserializer> _deserializer;
    ProtectedObj<PlayersList> _activePlayers;
    ProtectedObj<PlayersList> _backgroundPlayers;
};*/

RtpPacketsPlayer::RtpPacketsPlayer()
    : _timer("RtpPacketsPlayer")
{
}

RtpPacketsPlayer::~RtpPacketsPlayer()
{
    //LOCK_WRITE_PROTECTED_OBJ(_streams);
    //_streams->clear();
}

void RtpPacketsPlayer::AddStream(uint32_t ssrc, const RtpCodecMimeType& mime,
                                 RtpPacketsPlayerCallback* packetsCollector,
                                 const RtpPacketsInfoProvider* packetsInfoProvider)
{
    if (ssrc && packetsCollector && packetsInfoProvider) {
        /*LOCK_WRITE_PROTECTED_OBJ(_streams);
        if (_streams->end() == _streams->find(ssrc)) {
            if (WebMCodecs::IsSupported(mime)) {
                auto stream = std::make_shared<Stream>(ssrc, mime, packetsCollector,
                                                       packetsInfoProvider, &_timer);
                _streams.Ref()[ssrc] = std::move(stream);
            }
            else {
                // TODO: log error
            }
        }*/
    }
}

void RtpPacketsPlayer::RemoveStream(uint32_t ssrc)
{
    if (ssrc) {
        /*LOCK_WRITE_PROTECTED_OBJ(_streams);
        const auto it = _streams->find(ssrc);
        if (it != _streams->end()) {
            _streams->erase(it);
        }*/
    }
}

bool RtpPacketsPlayer::IsPlaying(uint32_t ssrc) const
{
    return false;
}

void RtpPacketsPlayer::Play(uint32_t ssrc, uint64_t mediaId,
                            const std::shared_ptr<MemoryBuffer>& buffer,
                            const void* userData)
{
    /*if (buffer && !buffer->IsEmpty()) {
        if (const auto stream = GetStream(ssrc)) {
            stream->StartMediaWriting();
            stream->WriteMediaPayload(buffer);
            stream->EndMediaWriting();
        }
    }*/
}

/*std::shared_ptr<RtpPacketsPlayer::Stream> RtpPacketsPlayer::GetStream(uint32_t ssrc) const
{
    if (ssrc) {
        LOCK_READ_PROTECTED_OBJ(_streams);
        const auto it = _streams->find(ssrc);
        if (it != _streams->end()) {
            return it->second;
        }
    }
    return nullptr;
}*/

/*RtpPacketsPlayer::TrackPlayer::TrackPlayer(uint32_t ssrc,
                                           RtpPacketsPlayerCallback* packetsCollector,
                                           const RtpPacketsInfoProvider* packetsInfoProvider,
                                           size_t trackIndex, MediaTimer* timer,
                                           std::unique_ptr<RtpPacketizer> packetizer)
    : MediaInfo(ssrc, packetsCollector, packetsInfoProvider)
    , _trackIndex(trackIndex)
    , _timer(timer)
    , _packetizer(std::move(packetizer))
{
}

RtpPacketsPlayer::TrackPlayer::~TrackPlayer()
{
    _timer->Stop(GetTimerEventId());
}

void RtpPacketsPlayer::TrackPlayer::Enque(PlayTask task)
{
    const auto startTimer = PlayTaskType::Started == task.GetType();
    {
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        _tasks->push(std::move(task));
    }
    if (startTimer) {
        _timer->SetTimeout(GetTimerEventId(), 0ULL);
        _timer->Start(GetTimerEventId(), false);
    }
}

RtpPacket* RtpPacketsPlayer::TrackPlayer::CreatePacket(const std::shared_ptr<const MediaFrame>& frame)
{
    if (frame) {
        if (const auto packet = _packetizer->AddFrame(frame)) {
            //if (!_initialRtpTimestamp) {
            //    _initialRtpTimestamp = GetLastOriginalRtpTimestamp();
            //}
            //_sequenceNumber = std::max(_sequenceNumber, GetLastOriginalRtpSeqNumber());
            //const uint32_t timestamp = _initialRtpTimestamp + frame->GetTimestamp();
            packet->SetSsrc(GetSsrc());
            //packet->SetSequenceNumber(++_sequenceNumber);
            //packet->SetTimestamp(std::max(timestamp, GetLastOriginalRtpTimestamp()));
            return packet;
        }
    }
    return nullptr;
}

void RtpPacketsPlayer::TrackPlayer::OnEvent()
{
    std::optional<PlayTask> task;
    {
        LOCK_WRITE_PROTECTED_OBJ(_tasks);
        if (!_tasks->empty()) {
            task = std::move(_tasks->front());
            _tasks->pop();
        }
    }
    if (task.has_value()) {
        switch (task->GetType()) {
            case PlayTaskType::Started:
                GetPacketsCollector()->OnPlayStarted(GetSsrc());
                break;
            case PlayTaskType::Frame:
                if (const auto frame = task->GetFrame()) {
                    const auto ts = frame->GetTimestamp();
                    if (0U == ts) { // 1st
                        // TODO: rewrite timeout calculations
                        // TODO: review timestamp type, must me more clarified (ms/us/nanosecs)
                        _timer->SetTimeout(GetTimerEventId(), 20U); // 20ms
                        //const auto diff = (frame->GetTimestamp()  - _playoutOffset) * 1000;
                        //_playoutOffset = frame->GetTimestamp();
                        //_timer->SetTimeout(GetTimerEventId(), diff / GetClockRate());
                    }
                    if (const auto packet = CreatePacket(frame)) {
                        GetPacketsCollector()->OnPlay(frame->GetTimestamp(), packet);
                    }
                }
                break;
            case PlayTaskType::Finished:
                GetPacketsCollector()->OnPlayFinished(GetSsrc());
                _timer->Stop(GetTimerEventId());
                break;
        }
    }
}

RtpPacketsPlayer::Stream::Stream(uint32_t ssrc,
                                 const RtpCodecMimeType& mime,
                                 RtpPacketsPlayerCallback* packetsCollector,
                                 const RtpPacketsInfoProvider* packetsInfoProvider,
                                 MediaTimer* timer)
    : MediaInfo(ssrc, packetsCollector, packetsInfoProvider)
    , _mime(mime)
    , _timer(timer)
{
}

RtpPacketsPlayer::Stream::~Stream()
{
    SetDeserializer(nullptr);
    ClearTrackPlayers(true);
    LOCK_WRITE_PROTECTED_OBJ(_backgroundPlayers);
    for (auto player : _backgroundPlayers.ConstRef()) {
        _timer->UnregisterTimer(player->GetTimerEventId());
    }
    _backgroundPlayers->clear();
}

void RtpPacketsPlayer::Stream::StartMediaWriting()
{
    SetDeserializer(std::make_shared<WebMDeserializer>());
}

void RtpPacketsPlayer::Stream::WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer) {
        if (const auto deserializer = std::atomic_load(&_deserializer)) {
            const auto result = deserializer->AddBuffer(buffer);
            SetResult(result, "media buffer deserialization");
            bool requestMediaFrames = false;
            if (MaybeOk(result)) {
                requestMediaFrames = FetchMediaInfo(deserializer);
            }
            if (requestMediaFrames && IsOk(result)) {
                DeserializeMediaFrames(deserializer);
            }
        }
    }
}

void RtpPacketsPlayer::Stream::EndMediaWriting()
{
    SetDeserializer(nullptr);
    ClearTrackPlayers(false);
}

void RtpPacketsPlayer::Stream::SetDeserializer(std::shared_ptr<MediaFrameDeserializer> deserializer)
{
    if (deserializer != std::atomic_exchange(&_deserializer, deserializer)) {
        ResetResult();
    }
}

void RtpPacketsPlayer::Stream::ClearTrackPlayers(bool destroy)
{
    LOCK_WRITE_PROTECTED_OBJ(_activePlayers);
    for (auto activePlayer : _activePlayers.Ref()) {
        activePlayer->Enque(false);
        if (destroy) {
            _timer->UnregisterTimer(activePlayer->GetTimerEventId());
            activePlayer.reset();
        }
        else {
            LOCK_WRITE_PROTECTED_OBJ(_backgroundPlayers);
            _backgroundPlayers->insert(activePlayer);
        }
    }
    _activePlayers->clear();
}

bool RtpPacketsPlayer::Stream::FetchMediaInfo(const std::shared_ptr<MediaFrameDeserializer>& deserializer)
{
    if (deserializer) {
        LOCK_WRITE_PROTECTED_OBJ(_activePlayers);
        if (_activePlayers->empty()) {
            if (const auto tracksCount = deserializer->GetTracksCount()) {
                for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                    const auto mime = deserializer->GetTrackMimeType(trackIndex);
                    if (mime.has_value() && mime.value() == _mime) {
                        std::unique_ptr<RtpPacketizer> packetizer;
                        switch (_mime.GetSubtype()) {
                            case RtpCodecMimeType::Subtype::OPUS:
                                packetizer = std::make_unique<RtpPacketizerOpus>();
                                break;
                            default:
                                break;
                        }
                        if (packetizer) {
                            auto player = std::make_shared<TrackPlayer>(GetSsrc(),
                                                                        GetPacketsCollector(),
                                                                        GetPacketsInfoProvider(),
                                                                        trackIndex, _timer,
                                                                        std::move(packetizer));
                            const auto timerId = _timer->RegisterTimer(player);
                            if (timerId) {
                                deserializer->SetClockRate(trackIndex, GetClockRate());
                                player->SetTimerEventId(timerId);
                                player->Enque(true);
                                _activePlayers->insert(std::move(player));
                            }
                            else {
                                // TODO: log error
                            }
                        }
                        else {
                            MS_ASSERT(false, "packetizer for [%s] not yet implemented", _mime.ToString().c_str());
                        }
                    }
                }
            }
        }
        return !_activePlayers->empty();
    }
    return false;
}

void RtpPacketsPlayer::Stream::DeserializeMediaFrames(const std::shared_ptr<MediaFrameDeserializer>& deserializer)
{
    if (deserializer) {
        LOCK_READ_PROTECTED_OBJ(_activePlayers);
        for (auto player : _activePlayers.ConstRef()) {
            MediaFrameDeserializeResult result = MediaFrameDeserializeResult::Success;
            for (const auto& frame : deserializer->ReadNextFrames(player->GetTrackIndex(), &result)) {
                player->Enque(frame);
            }
            player->SetResult(result, "read of deserialized frames");
        }
    }
}*/

} // namespace RTC

namespace {

MediaInfo::MediaInfo(uint32_t ssrc, RtpPacketsPlayerCallback* packetsCollector,
                     const RtpPacketsInfoProvider* packetsInfoProvider)
    : _ssrc(ssrc)
    , _packetsCollector(packetsCollector)
    , _packetsInfoProvider(packetsInfoProvider)
{
}

void MediaInfo::SetResult(MediaFrameDeserializeResult result, const char* operationName)
{
    if (result != _lastResult.exchange(result) && !MaybeOk(result)) {
        MS_ERROR_STD("%s operation %s failed: %s", GetDescription().c_str(),
                     operationName, ToString(result));
    }
}

/*uint16_t MediaInfo::GetLastOriginalRtpSeqNumber() const
{
    return GetPacketsInfoProvider()->GetLastOriginalRtpSeqNumber(GetSsrc());
}

uint32_t MediaInfo::GetLastOriginalRtpTimestamp() const
{
    return GetPacketsInfoProvider()->GetLastOriginalRtpTimestamp(GetSsrc());
}*/

uint32_t MediaInfo::GetClockRate() const
{
    return GetPacketsInfoProvider()->GetClockRate(GetSsrc());
}

PlayTask::PlayTask(bool started)
    : _data(started)
{
}

PlayTask::PlayTask(const std::shared_ptr<const MediaFrame>& frame)
    : _data(frame)
{
}

PlayTaskType PlayTask::GetType() const
{
    if (const auto started = std::get_if<bool>(&_data)) {
        return *started ? PlayTaskType::Started : PlayTaskType::Finished;
    }
    return PlayTaskType::Frame;
}

std::shared_ptr<const MediaFrame> PlayTask::GetFrame() const
{
    if (const auto frame = std::get_if<std::shared_ptr<const MediaFrame>>(&_data)) {
        return *frame;
    }
    return nullptr;
}

}
