#define MS_CLASS "RTC::FileReader"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/Buffers/BufferAllocator.hpp"
#include "Logger.hpp"
#include <algorithm> // for std::min/max
#include <inttypes.h>

namespace RTC
{

class FileReader::StartEndNotifier
{
public:
    StartEndNotifier(FileReader* source);
    ~StartEndNotifier();
private:
    FileReader* const _source;
};

FileReader::FileReader(const std::weak_ptr<BufferAllocator>& allocator,
                       bool loop, size_t chunkSize)
    : Base(allocator)
    , _loop(loop)
    , _chunkSize(chunkSize)
{
}

FileReader::~FileReader()
{
    Stop();
}

bool FileReader::Open(const std::string_view& fileNameUtf8, int* error)
{
    if (Base::Open(fileNameUtf8, true, error)) {
        //std::min<size_t>(_fileSize, std::max<size_t>(chunkSize, 1024UL))
        const auto fileSize = GetFileSize(GetHandle());
        _fileSize = fileSize;
        _actualChunkSize = std::min<size_t>(fileSize, std::max<size_t>(_chunkSize, 1024U));
        return true;
    }
    return false;
}

bool FileReader::IsOpen() const
{
    return Base::IsOpen() && _fileSize > 0LL;
}

std::shared_ptr<Buffer> FileReader::ReadAll(const std::string_view& fileNameUtf8,
                                            const std::weak_ptr<BufferAllocator>& allocator)
{
    if (const auto handle = Base::OpenFile(fileNameUtf8, true)) {
        auto size = GetFileSize(handle);
        if (size > 0) {
            try {
                if (const auto buffer = RTC::AllocateBuffer(size, allocator)) {
                    return RTC::ReallocateBuffer(FileRead(handle, buffer), buffer, allocator);
                }
            }
            catch (const std::bad_alloc& e) {
                MS_ERROR_STD("unable to allocate %" PRId64 " bytes for file data", size);
            }
        }
        else {
            MS_WARN_DEV_STD("no data for read because file %s is empty", fileNameUtf8.c_str());
        }
    }
    return nullptr;
}

bool FileReader::IsValidForRead(const std::string_view& fileNameUtf8)
{
    bool valid = false;
    if (const auto handle = Base::OpenFile(fileNameUtf8, true)) {
        auto size = GetFileSize(handle);
        valid = size > 0;
    }
    return valid;
}

void FileReader::OnSinkWasAdded(MediaSink* sink, bool first)
{
    Base::OnSinkWasAdded(sink, first);
    if (first && IsOpen() && !_thread.joinable()) {
        _thread = std::thread(std::bind(&FileReader::Run, this));
    }
}

void FileReader::OnSinkWasRemoved(MediaSink* sink, bool last)
{
    if (last) {
        Stop();
    }
    Base::OnSinkWasRemoved(sink, last);
}

void FileReader::Run()
{
   if (!IsStopRequested()) {
        bool operationDone = false, ok = true;
        for(; (!operationDone || _loop) && !IsStopRequested();) {
            ok = ReadContent();
            if (ok) {
                operationDone = true;
                if (_loop) {
                    ok = SeekToStart();
                    if (!ok) { // seek to start
                        MS_WARN_DEV_STD("Failed seek to beginning of file");
                    }
                }
            }
            if (!ok) {
                break;
            }
        }
    }
    SeekToStart();
}

void FileReader::Stop()
{
    if (!_stopRequested.exchange(true)) {
        if (_thread.joinable()) {
            _thread.join();
        }
        _stopRequested = false;
    }
}

bool FileReader::ReadContent()
{
    if (!IsStopRequested()) {
        const StartEndNotifier notifier(this);
        bool eof = false, ok = true;
        while (!IsStopRequested() && ok && !eof) {
            WriteMediaSinksPayload(ReadBuffer(eof, ok));
        }
        if (!ok) {
            return false;
        }
    }
    return !IsStopRequested();
}

bool FileReader::SeekToStart()
{
    return FileSeek(GetHandle(), SEEK_SET, 0L);
}

std::shared_ptr<Buffer> FileReader::ReadBuffer(bool& eof, bool& ok) const
{
    std::shared_ptr<Buffer> buffer;
    const auto handle = GetHandle();
    const auto chunkSize = _actualChunkSize.load();
    const auto fileSize = _fileSize.load();
    if (handle && chunkSize && fileSize) {
        auto buffer = AllocateBuffer(chunkSize);
        const auto size = FileRead(handle, buffer);
        if (size) {
            buffer = ReallocateBuffer(size, buffer);
        }
        else {
            ok = 0 == errno;
            if (!ok) {
                MS_WARN_DEV_STD("Unable to read file chunk, size %zu, error code %d", chunkSize, errno);
            }
        }
        if (ok) {
            eof = fileSize == FileTell(handle);
        }
    }
    else {
        eof = true;
    }
    return buffer;
}

int64_t FileReader::GetFileSize(const std::shared_ptr<FILE>& handle)
{
    int64_t len = 0LL;
    if (handle && FileSeek(handle, SEEK_END)) {
        len = FileTell(handle);
        if (len < 0LL) {
            len = 0LL;
        }
        if (!FileSeek(handle, SEEK_SET)) {
            len = 0LL;
        }
    }
    return len;
}

int64_t FileReader::FileTell(const std::shared_ptr<FILE>& handle)
{
    if (handle) {
#ifdef _MSC_VER
        return ::_ftelli64(handle.get());
#else
        return ::ftell(handle.get());
#endif
    }
    return -1;
}

bool FileReader::FileSeek(const std::shared_ptr<FILE>& handle, int command, long offset)
{
    if (handle && offset >= 0L) {
#ifdef _MSC_VER
        return 0 == ::_fseeki64(handle.get(), offset, command);
#else
        ::fseek(handle.get(), offset, command);
        return true;
#endif
    }
    return false;
}

size_t FileReader::FileRead(const std::shared_ptr<FILE>& handle, const std::shared_ptr<Buffer>& to)
{
    if (handle && to && !to->IsEmpty()) {
        return ::fread(to->GetData(), 1UL, to->GetSize(), handle.get());
    }
    return 0UL;
}

FileReader::StartEndNotifier::StartEndNotifier(FileReader* source)
    : _source(source)
{
    _source->StartMediaSinksWriting();
}

FileReader::StartEndNotifier::~StartEndNotifier()
{
    _source->EndMediaSinksWriting();
}

} // namespace RTC
