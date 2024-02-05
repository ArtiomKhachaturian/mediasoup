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

void MediaSourceImpl::StartMediaSinksWriting(uint32_t ssrc)
{
    _sinks.InvokeMethod(&MediaSink::StartMediaWriting, ssrc);
}

void MediaSourceImpl::WriteMediaSinksPayload(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer) {
        _sinks.InvokeMethod(&MediaSink::WriteMediaPayload, buffer);
    }
}

void MediaSourceImpl::EndMediaSinksWriting()
{
    _sinks.InvokeMethod(&MediaSink::EndMediaWriting);
}


} // namespace RTC
