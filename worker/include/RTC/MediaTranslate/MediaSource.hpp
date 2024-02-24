#pragma once
#include "RTC/ObjectId.hpp"

namespace RTC
{

class MediaSink;

class MediaSource : public ObjectId
{
public:
    virtual ~MediaSource() = default;
    virtual bool AddSink(MediaSink* sink) = 0;
    virtual bool RemoveSink(MediaSink* sink) = 0;
    virtual void RemoveAllSinks() = 0;
    virtual bool HasSinks() const = 0;
};

} // namespace RTC
