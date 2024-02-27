#pragma once

namespace RTC
{

enum class MediaFrameDeserializeResult {
    // Unexpected/unrecoverable parsing error.
   ParseError = -3,
   // Cannot allocate memory.
   OutOfMemory = -2,
   // Invalid argument passed.
   InvalidArg = -1,
   // OK
   Success = 0,
   // Parsing failed because data was exhausted before the end of an
   // element.  Add more data to your buffer.
   NeedMoreData = 1,
};

inline bool IsOk(MediaFrameDeserializeResult result) {
    return MediaFrameDeserializeResult::Success == result;
}

inline bool MaybeOk(MediaFrameDeserializeResult result) {
    return MediaFrameDeserializeResult::NeedMoreData == result || IsOk(result);
}

const char* ToString(MediaFrameDeserializeResult result);

} // namespace RTC
