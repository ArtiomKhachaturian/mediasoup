#pragma once
#include "RTC/Listeners.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class MediaSink;
class MemoryBuffer;

class MediaSourceImpl : public MediaSource
{
public:
    // impl. of MediaSource
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final { return !_sinks.IsEmpty(); }
    size_t GetSinksCout() const final { return _sinks.GetSize(); }
protected:
    MediaSourceImpl() = default;
    virtual bool IsSinkValid(const MediaSink* sink) const { return nullptr != sink; }
    virtual void OnSinkWasAdded(MediaSink* /*sink*/, bool /*first*/) {}
    virtual void OnSinkWasRemoved(MediaSink* /*sink*/, bool /*last*/) {}
    void StartMediaSinksWriting(uint32_t ssrc);
    void WriteMediaSinksPayload(const std::shared_ptr<MemoryBuffer>& buffer);
    void EndMediaSinksWriting();
private:
    Listeners<MediaSink*> _sinks;
};

} // namespace RTC
