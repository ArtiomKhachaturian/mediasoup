#pragma once

namespace RTC
{

enum class MediaFrameDeserializeResult {
    // Unexpected/unrecoverable parsing error.
   ParseError = -3,
   // Cannot allocate memory.
   OutOfMemory = -2,
   // Invalid argument passed to 'AddBuffer'.
   InvalidArg = -1,
   // OK
   Success = 0,
   // Parsing failed because data was exhausted before the end of an
   // element.  Add more data to your buffer.
   NeedMoreData = 1,
};

} // namespace RTC
