#include "RTC/MediaTranslate/WebM/MkvReadResult.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"

namespace RTC
{

const char* MkvReadResultToString(MkvReadResult result)
{
    switch (result) {
        case MkvReadResult::InvalidInputArg:
            return "invalid input argument";
        case MkvReadResult::OutOfMemory:
            return "out of memory";
        case MkvReadResult::ParseFailed:
            return "parse failed";
        case MkvReadResult::FileFormatInvalid:
            return "invalid file format";
        case MkvReadResult::BufferNotFull:
            return "buffer not full";
        case MkvReadResult::Success:
            return "ok";
        case MkvReadResult::NoMoreClusters:
            return "no more clusters";
        default:
            break;
    }
    return "unknown error";
}

MediaFrameDeserializeResult FromMkvReadResult(MkvReadResult result)
{
    switch (result) {
        case MkvReadResult::InvalidInputArg:
            return MediaFrameDeserializeResult::InvalidArg;
        case MkvReadResult::OutOfMemory:
            return MediaFrameDeserializeResult::OutOfMemory;
        case MkvReadResult::BufferNotFull:
        case MkvReadResult::NoMoreClusters:
            return MediaFrameDeserializeResult::NeedMoreData;
        case MkvReadResult::Success:
            return MediaFrameDeserializeResult::Success;
        default:
            break;
    }
    return MediaFrameDeserializeResult::ParseError;
}

} // namespace RTC
