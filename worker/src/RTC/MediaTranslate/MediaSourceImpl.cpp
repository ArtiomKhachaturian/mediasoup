#include "RTC/MediaTranslate/MediaSourceImpl.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"

namespace RTC
{

bool MediaSourceImpl::AddSink(MediaSink* sink)
{
    if (IsSinkValid(sink) && _sinks.Add(sink)) {
        OnSinkWasAdded(sink, 1UL == _sinks.GetSize());
        return true;
    }
    return false;
}

bool MediaSourceImpl::RemoveSink(MediaSink* sink)
{
    if (IsSinkValid(sink) && _sinks.Remove(sink)) {
        OnSinkWasRemoved(sink, _sinks.IsEmpty());
        return true;
    }
    return false;
}

void MediaSourceImpl::RemoveAllSinks()
{
    _sinks.Clear();
}

void MediaSourceImpl::StartMediaSinksWriting(bool restart)
{
    _sinks.InvokeMethod(&MediaSink::StartMediaWriting, restart);
}

void MediaSourceImpl::WriteMediaSinksPayload(uint32_t ssrc, const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (buffer) {
        _sinks.InvokeMethod(&MediaSink::WriteMediaPayload, ssrc, buffer);
    }
}

void MediaSourceImpl::EndMediaSinksWriting()
{
    _sinks.InvokeMethod(&MediaSink::EndMediaWriting);
}


} // namespace RTC
