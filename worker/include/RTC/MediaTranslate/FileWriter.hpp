#pragma once
#include "RTC/MediaTranslate/FileDevice.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"

namespace RTC
{

class FileWriter : public FileDevice<MediaSink>
{
public:
    FileWriter(const std::string_view& fileNameUtf8, int* error = nullptr);
    // Write any buffered data to the underlying file. Returns true on success,
    // false on write error. Note: Flushing when closing, is not required.
    bool Flush();
    // impl. of MediaSink
    bool IsLiveMode() const final { return false; }
    void WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer) final;
};

} // namespace RTC
