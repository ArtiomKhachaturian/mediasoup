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

void MediaSource::StartMediaSinksWriting(bool restart, uint32_t startTimestamp) noexcept
{
    _sinks.InvokeMethod(&MediaSink::StartMediaWriting, restart, startTimestamp);
}

void MediaSource::WriteMediaSinksPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    _sinks.InvokeMethod(&MediaSink::WriteMediaPayload, buffer);
}

void MediaSource::EndMediaSinksWriting() noexcept
{
    _sinks.InvokeMethod(&MediaSink::EndMediaWriting);
}


} // namespace RTC
