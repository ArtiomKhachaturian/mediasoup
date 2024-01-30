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

void MediaSource::RemoveAllSinks()
{
    _sinks.Clear();
}

void MediaSource::StartMediaSinksWriting(bool restart)
{
    _sinks.InvokeMethod(&MediaSink::StartMediaWriting, restart);
}

void MediaSource::WriteMediaSinksPayload(uint32_t ssrc, const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (buffer) {
        _sinks.InvokeMethod(&MediaSink::WriteMediaPayload, ssrc, buffer);
    }
}

void MediaSource::EndMediaSinksWriting()
{
    _sinks.InvokeMethod(&MediaSink::EndMediaWriting);
}


} // namespace RTC
