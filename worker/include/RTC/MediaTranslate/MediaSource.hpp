#pragma once
#include "RTC/Listeners.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class MediaSink;
class MemoryBuffer;

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
    void StartMediaSinksWriting(bool restart) noexcept;
    void WriteMediaSinksPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept;
    void EndMediaSinksWriting() noexcept;
private:
    Listeners<MediaSink*> _sinks;
};

} // namespace RTC
