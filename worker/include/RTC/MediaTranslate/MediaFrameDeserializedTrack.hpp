#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include <cstdint>
#include <optional>

namespace RTC
{

class MediaFrame;

class MediaFrameDeserializedTrack
{
public:
	virtual ~MediaFrameDeserializedTrack() = default;
	virtual std::optional<MediaFrame> NextFrame(size_t payloadOffset, bool skipPayload) = 0;
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
