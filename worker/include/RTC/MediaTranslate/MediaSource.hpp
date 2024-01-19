#pragma once
#include "RTC/Listeners.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class MediaSink;
class MemoryBuffer;
class RtpMediaFrame;
struct RtpMediaPacketInfo;

class MediaSource
{
public:
    virtual ~MediaSource() = default;
    void AddSink(MediaSink* sink);
    void RemoveSink(MediaSink* sink);
    bool HasSinks() const { return !_sinks.IsEmpty(); }
protected:
    MediaSource() = default;
    virtual bool IsSinkValid(const MediaSink* sink) const { return nullptr != sink; }
    virtual void OnFirstSinkAdded() {}
    virtual void OnLastSinkRemoved() {}
    void StartMediaSinksStream(bool restart) noexcept;
    void BeginWriteMediaSinksPayload(uint32_t ssrc,
                                     const std::vector<RtpMediaPacketInfo>& packets) noexcept;
    void BeginWriteMediaSinksPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) noexcept;
    void WritePayloadToMediaSinks(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept;
    void EndWriteMediaSinksPayload(uint32_t ssrc, const std::vector<RtpMediaPacketInfo>& packets,
                                   bool ok) noexcept;
    void EndWriteMediaSinksPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame,
                                   bool ok) noexcept;
    void EndMediaSinksStream(bool failure) noexcept;
private:
    Listeners<MediaSink*> _sinks;
};

} // namespace RTC
