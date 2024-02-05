#pragma once
#include <cstdint>

namespace RTC
{

class MediaSink;

class MediaSource
{
public:
    virtual ~MediaSource() = default;
    virtual bool AddSink(MediaSink* sink) = 0;
    virtual bool RemoveSink(MediaSink* sink) = 0;
    virtual void RemoveAllSinks() = 0;
    virtual bool HasSinks() const = 0;
    virtual size_t GetSinksCout() const = 0;
};

} // namespace RTC
