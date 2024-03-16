#include "RTC/MediaTranslate/MediaFrameConfig.hpp"
#include "RTC/Buffers/BufferAllocator.hpp"

namespace RTC
{

MediaFrameConfig::~MediaFrameConfig()
{
}

const std::shared_ptr<const Buffer>& MediaFrameConfig::GetCodecSpecificData() const
{
    return _codecSpecificData;
}

void MediaFrameConfig::SetCodecSpecificData(const std::shared_ptr<const Buffer>& data)
{
    _codecSpecificData = data;
}

void MediaFrameConfig::SetCodecSpecificData(const uint8_t* data, size_t len,
                                            const std::shared_ptr<BufferAllocator>& allocator)
{
    SetCodecSpecificData(RTC::AllocateBuffer(len, data, allocator));
}

bool MediaFrameConfig::IsCodecSpecificDataEqual(const MediaFrameConfig& config) const
{
    bool equal = this == &config;
    if (!equal && GetCodecSpecificData()) {
        equal = GetCodecSpecificData()->IsEqual(config.GetCodecSpecificData());
    }
    return equal;
}

} // namespace RTC
