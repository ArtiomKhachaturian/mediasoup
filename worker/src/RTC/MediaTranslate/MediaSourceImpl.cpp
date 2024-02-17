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

void MediaSourceImpl::Commmit(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer) {
        StartMediaSinksWriting();
        WriteMediaSinksPayload(buffer);
        EndMediaSinksWriting();
    }
}

void MediaSourceImpl::StartMediaSinksWriting()
{
    _sinks.InvokeMethod(&MediaSink::StartMediaWriting, *this);
}

void MediaSourceImpl::WriteMediaSinksPayload(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer) {
        _sinks.InvokeMethod(&MediaSink::WriteMediaPayload, *this, buffer);
    }
}

void MediaSourceImpl::EndMediaSinksWriting()
{
    _sinks.InvokeMethod(&MediaSink::EndMediaWriting, *this);
}


} // namespace RTC
