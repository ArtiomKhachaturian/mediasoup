#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"

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

void MediaSource::BeginWriteMediaSinksPayload(uint32_t ssrc) noexcept
{
    _sinks.InvokeMethod(&MediaSink::BeginWriteMediaPayload, ssrc);
}

void MediaSource::WriteMediaSinksPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer) {
        _sinks.InvokeMethod(&MediaSink::WriteMediaPayload, buffer);
    }
}

void MediaSource::EndWriteMediaSinksPayload(uint32_t ssrc, bool ok) noexcept
{
    _sinks.InvokeMethod(&MediaSink::EndWriteMediaPayload, ssrc, ok);
}

void MediaSource::EndMediaSinksStream(bool failure) noexcept
{
    _sinks.InvokeMethod(&MediaSink::EndStream, failure);
}

} // namespace RTC
