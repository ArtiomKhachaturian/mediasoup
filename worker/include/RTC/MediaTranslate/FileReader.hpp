#pragma once
#include "RTC/MediaTranslate/FileDevice.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include <atomic>
#include <thread>

namespace RTC
{

class FileReader : public FileDevice<MediaSource>
{
    class StartEndNotifier;
public:
	FileReader(const std::string_view& fileNameUtf8,
               bool loop = true, size_t chunkSize = 1024U * 1024U, // 1mb
               int* error = nullptr);
    ~FileReader() final;
    void SetSsrc(uint32_t ssrc) { _ssrc = ssrc; }
    // overrides of FileDevice<>
    bool IsOpen() const final;
protected:
    // overrides of MediaSource
    void OnFirstSinkAdded() final;
    void OnLastSinkRemoved() final;
private:
    bool IsStopRequested() const { return _stopRequested.load(); }
    void Stop();
    bool StartRead(bool restart); // return false if error or stop requested
    std::shared_ptr<const MemoryBuffer> ReadBuffer(bool& eof, bool* error = nullptr) const;
    static int64_t GetFileSize(FILE* handle);
    static int64_t FileTell(FILE* handle);
    static bool FileSeek(FILE* handle, int command, long offset = 0L);
private:
	const bool _loop;
    const int64_t _fileSize;
    const size_t _chunkSize;
    std::atomic_bool _stopRequested = false;
    std::atomic<uint32_t> _ssrc = 0U;
    std::thread _thread;
};

} // namespace RTC
