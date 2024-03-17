#pragma once
#include <mkvparser/mkvparser.h>

namespace RTC
{

enum class MediaFrameDeserializeResult;

enum class MkvReadResult {
    UnknownError        = mkvparser::E_PARSE_FAILED - 300,
    InvalidInputArg     = mkvparser::E_PARSE_FAILED - 200,
    OutOfMemory         = mkvparser::E_PARSE_FAILED - 100,
    ParseFailed         = mkvparser::E_PARSE_FAILED,
    FileFormatInvalid   = mkvparser::E_FILE_FORMAT_INVALID,
    BufferNotFull       = mkvparser::E_BUFFER_NOT_FULL,
    Success             = 0L,
    NoMoreClusters      = 1L // EOF ?
};

template<typename T>
inline MkvReadResult ToMkvReadResult(T result) {
    switch (result) {
        case mkvparser::E_PARSE_FAILED:
            return MkvReadResult::ParseFailed;
        case mkvparser::E_FILE_FORMAT_INVALID:
            return MkvReadResult::FileFormatInvalid;
        case mkvparser::E_BUFFER_NOT_FULL:
            return MkvReadResult::BufferNotFull;
        case 1:
            return MkvReadResult::NoMoreClusters;
        default:
            if (result >= 0) {
                return MkvReadResult::Success;
            }
            break;
    }
    return MkvReadResult::UnknownError;
}

inline bool IsOk(MkvReadResult result) {
    return MkvReadResult::Success == result;
}

inline bool MaybeOk(MkvReadResult result) {
    switch (result) {
        case MkvReadResult::BufferNotFull:
        case MkvReadResult::NoMoreClusters:
            return true;
        default:
            break;
    }
    return IsOk(result);
}

const char* MkvReadResultToString(MkvReadResult result);
MediaFrameDeserializeResult FromMkvReadResult(MkvReadResult result);

template<typename T>
inline const char* MkvReadResultToString(T result) {
    return ToString(ToMkvReadResult(result));
}

} // namespace RTC
