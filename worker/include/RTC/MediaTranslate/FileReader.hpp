#pragma once
#include "RTC/MediaTranslate/FileDevice.hpp"
#include "RTC/MediaTranslate/MediaSourceImpl.hpp"
#include <atomic>
#include <thread>

namespace RTC
{


class FileReader : public FileDevice<MediaSourceImpl>
{
    class StartEndNotifier;
    using Base = FileDevice<MediaSourceImpl>;
public:
	FileReader(const std::weak_ptr<BufferAllocator>& allocator,
               bool loop = true,
               size_t chunkSize = 1024U * 1024U /* 1mb */);
    ~FileReader() final;
    static std::shared_ptr<Buffer> ReadAll(const std::string_view& fileNameUtf8,
                                           const std::weak_ptr<BufferAllocator>& allocator = std::weak_ptr<BufferAllocator>());
    static bool IsValidForRead(const std::string_view& fileNameUtf8);
    bool Open(const std::string_view& fileNameUtf8, int* error = nullptr);
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
    std::shared_ptr<Buffer> ReadBuffer(bool& eof, bool& ok) const;
    static int64_t GetFileSize(const std::shared_ptr<FILE>& handle);
    static int64_t FileTell(const std::shared_ptr<FILE>& handle);
    static bool FileSeek(const std::shared_ptr<FILE>& handle, int command, long offset = 0L);
    static size_t FileRead(const std::shared_ptr<FILE>& handle, const std::shared_ptr<Buffer>& to);
private:
	const bool _loop;
    const size_t _chunkSize;
    std::atomic<int64_t> _fileSize = 0ULL;
    std::atomic<size_t> _actualChunkSize = 0U;
    std::atomic_bool _stopRequested = false;
    std::thread _thread;
};

} // namespace RTC
