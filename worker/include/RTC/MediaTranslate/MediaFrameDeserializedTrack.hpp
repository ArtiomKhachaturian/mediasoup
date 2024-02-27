#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include <cstdint>
#include <memory>

namespace RTC
{

class BufferAllocator;
class MediaFrame;

class MediaFrameDeserializedTrack
{
public:
	virtual ~MediaFrameDeserializedTrack() = default;
	virtual std::shared_ptr<MediaFrame> NextFrame(size_t payloadOffset,
                                                  const std::weak_ptr<BufferAllocator>& allocator) = 0;
    void SetClockRate(uint32_t clockRate);
	uint32_t GetClockRate() const { return _clockRate; }
	MediaFrameDeserializeResult GetLastResult() const { return _lastResult; }
    size_t GetLastPayloadSize() const { return _lastPayloadSize; }
protected:
	void SetLastResult(MediaFrameDeserializeResult result) { _lastResult = result; }
    void SetLastPayloadSize(size_t size) { _lastPayloadSize = size; }
private:
	uint32_t _clockRate = 0U;
    size_t _lastPayloadSize = 0U;
	MediaFrameDeserializeResult _lastResult = MediaFrameDeserializeResult::Success;
};

} // namespace RTC
