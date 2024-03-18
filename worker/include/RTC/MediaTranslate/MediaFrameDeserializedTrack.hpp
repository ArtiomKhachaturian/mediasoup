#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include <cstdint>

namespace RTC
{

class MediaFrameDeserializedTrack
{
public:
	virtual ~MediaFrameDeserializedTrack() = default;
	virtual MediaFrame NextFrame(size_t payloadOffset, bool skipPayload,
                                 size_t payloadExtraSize = 0U) = 0;
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
