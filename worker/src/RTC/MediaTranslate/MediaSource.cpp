#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"

namespace RTC
{

void MediaSource::AddSink(MediaSink* sink)
{
    if (IsSinkValid(sink) && _sinks.Add(sink) && 1UL == _sinks.GetSize()) {
        OnFirstSinkAdded();
    }
}

void MediaSource::RemoveSink(MediaSink* sink)
{
    if (IsSinkValid(sink) && _sinks.Remove(sink) && _sinks.IsEmpty()) {
        OnLastSinkRemoved();
    }
}

void MediaSource::StartMediaSinksStream(bool restart) noexcept
{
    _sinks.InvokeMethod(&MediaSink::StartStream, restart);
}

void MediaSource::BeginWriteMediaSinksPayload(uint32_t ssrc,
                                              const std::vector<RtpMediaPacketInfo>& packets) noexcept
{
    if (!packets.empty()) {
        _sinks.InvokeMethod(&MediaSink::BeginWriteMediaPayload, ssrc, packets);
    }
}

void MediaSource::BeginWriteMediaSinksPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame) noexcept
{
    if (mediaFrame) {
        BeginWriteMediaSinksPayload(mediaFrame->GetSsrc(), mediaFrame->GetPacketsInfo());
    }
}

void MediaSource::WritePayloadToMediaSinks(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer) {
        _sinks.InvokeMethod(&MediaSink::Write, buffer);
    }
}

void MediaSource::EndWriteMediaSinksPayload(uint32_t ssrc,
                                            const std::vector<RtpMediaPacketInfo>& packets,
                                            bool ok) noexcept
{
    if (!packets.empty()) {
        _sinks.InvokeMethod(&MediaSink::EndWriteMediaPayload, ssrc, packets, ok);
    }
}

void MediaSource::EndWriteMediaSinksPayload(const std::shared_ptr<const RtpMediaFrame>& mediaFrame,
                                            bool ok) noexcept
{
    if (mediaFrame) {
        EndWriteMediaSinksPayload(mediaFrame->GetSsrc(), mediaFrame->GetPacketsInfo(), ok);
    }
}

void MediaSource::EndMediaSinksStream(bool failure) noexcept
{
    _sinks.InvokeMethod(&MediaSink::EndStream, failure);
}

} // namespace RTC
