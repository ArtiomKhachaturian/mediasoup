#pragma once
#include "RTC/MediaTranslate/FileDevice.hpp"
#include "RTC/MediaTranslate/MediaSourceImpl.hpp"
#include <atomic>
#include <thread>
#include <vector>

namespace RTC
{

class FileReader : public FileDevice<MediaSourceImpl>
{
    class StartEndNotifier;
    using Base = FileDevice<MediaSourceImpl>;
public:
	FileReader(const std::string_view& fileNameUtf8,
               bool loop = true, size_t chunkSize = 1024U * 1024U, // 1mb
               int* error = nullptr);
    ~FileReader() final;
    static std::vector<uint8_t> ReadAllAsBinary(const std::string_view& fileNameUtf8);
    static std::shared_ptr<MemoryBuffer> ReadAllAsBuffer(const std::string_view& fileNameUtf8);
    static bool IsValidForRead(const std::string_view& fileNameUtf8);
    // overrides of FileDevice<>
    bool IsOpen() const final;
protected:
    // overrides of MediaSource
    void OnSinkWasAdded(MediaSink* sink, bool first) final;
    void OnSinkWasRemoved(MediaSink* sink, bool last) final;
private:
    bool IsStopRequested() const { return _stopRequested.load(); }
    void Run();
    void Stop();
    bool ReadContent(); // return false if error or stop requested
    bool SeekToStart();
    std::shared_ptr<MemoryBuffer> ReadBuffer(bool& eof, bool& ok) const;
    static int64_t GetFileSize(FILE* handle);
    static int64_t FileTell(FILE* handle);
    static bool FileSeek(FILE* handle, int command, long offset = 0L);
    static size_t FileRead(FILE* handle, std::vector<uint8_t>& to);
private:
	const bool _loop;
    const int64_t _fileSize;
    const size_t _chunkSize;
    std::atomic_bool _stopRequested = false;
    std::thread _thread;
};

} // namespace RTC
