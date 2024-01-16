#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <vector>

namespace RTC
{

class MediaFrameConfig;
class AudioFrameConfig;
class VideoFrameConfig;
class MemoryBuffer;

class MediaFrame
{
public:
    MediaFrame(const RtpCodecMimeType& mimeType);
    virtual ~MediaFrame();
    // add-methods return false if input arguments is incorrect: null or empty payload
    bool AddPayload(std::vector<uint8_t> payload);
    bool AddPayload(const uint8_t* data, size_t len, const std::allocator<uint8_t>& payloadAllocator = {});
    bool AddPayload(const std::shared_ptr<const MemoryBuffer>& payload);
	bool IsEmpty() const { return _payloads.empty(); }
	const std::vector<std::shared_ptr<const MemoryBuffer>>& GetPayloads() const;
	// expensive operation, in difference from [GetPayloads] returns continuous area of payload data
    std::shared_ptr<const MemoryBuffer> GetPayload() const;
    size_t GetPayloadSize() const { return _payloadSize; }
    // common properties
    const RtpCodecMimeType& GetMimeType() const { return _mimeType; }
    bool IsAudio() const { return GetMimeType().IsAudioCodec(); }
    void SetKeyFrame(bool keyFrame);
    bool IsKeyFrame() const { return _keyFrame; }
    // audio configuration
    void SetAudioConfig(const std::shared_ptr<const AudioFrameConfig>& config);
    std::shared_ptr<const AudioFrameConfig> GetAudioConfig() const;
    // video configuration
    void SetVideoConfig(const std::shared_ptr<const VideoFrameConfig>& config);
    std::shared_ptr<const VideoFrameConfig> GetVideoConfig() const;
private:
	const RtpCodecMimeType _mimeType;
    bool _keyFrame = false;
    std::vector<std::shared_ptr<const MemoryBuffer>> _payloads;
    size_t _payloadSize = 0UL;
    std::shared_ptr<const MediaFrameConfig> _config;
};

} // namespace RTC	
