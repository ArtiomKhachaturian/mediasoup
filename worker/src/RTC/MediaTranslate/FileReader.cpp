#define MS_CLASS "RTC::FileReader"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

class FileReader::StartEndNotifier
{
public:
    StartEndNotifier(FileReader* source, bool restart);
    ~StartEndNotifier();
private:
    FileReader* const _source;
};

FileReader::FileReader(const std::string_view& fileNameUtf8,
                       bool loop, size_t chunkSize, int* error)
    : Base(fileNameUtf8, true, error)
    , _loop(loop)
    , _fileSize(GetFileSize(GetHandle()))
    , _chunkSize(std::min<size_t>(_fileSize, std::max(chunkSize, 1024UL)))
{
}

FileReader::~FileReader()
{
    Stop();
}

bool FileReader::IsOpen() const
{
    return Base::IsOpen() && _fileSize > 0LL;
}

void FileReader::OnSinkWasAdded(MediaSink* sink, bool first)
{
    Base::OnSinkWasAdded(sink, first);
    if (first && IsOpen() && !_thread.joinable()) {
        _thread = std::thread([this]() {
            if (!IsStopRequested()) {
                bool operationDone = false;
                for(; (!operationDone || _loop) && !IsStopRequested();) {
                    if (ReadContent(operationDone)) {
                        operationDone = true;
                        if (_loop) { // seek to start
                            if (!FileSeek(GetHandle(), SEEK_SET, 0L)) {
                                MS_WARN_DEV_STD("Failed seek to beginning of file");
                                break;
                            }
                        }
                    }
                    else {
                        break;
                    }
                }
            }
        });
    }
}

void FileReader::OnSinkWasRemoved(MediaSink* sink, bool last)
{
    if (last) {
        Stop();
    }
    Base::OnSinkWasRemoved(sink, last);
}

void FileReader::Stop()
{
    if (_thread.joinable() && !_stopRequested.exchange(true)) {
        _thread.join();
        _stopRequested = false;
    }
}

bool FileReader::ReadContent(bool restart)
{
    if (!IsStopRequested()) {
        const StartEndNotifier notifier(this, restart);
        bool eof = false, ok = true;
        while (!IsStopRequested() && ok && !eof) {
            if (const auto buffer = ReadBuffer(eof, ok)) {
                WriteMediaSinksPayload(_ssrc.load(), buffer);
            }
        }
        if (!ok) {
            return false;
        }
    }
    return !IsStopRequested();
}

std::shared_ptr<const MemoryBuffer> FileReader::ReadBuffer(bool& eof, bool& ok) const
{
    std::shared_ptr<const MemoryBuffer> buffer;
    if (const auto handle = GetHandle()) {
        std::vector<uint8_t> chunk;
        chunk.resize(_chunkSize, 0);
        const auto size = ::fread(chunk.data(), 1UL, chunk.size(), handle);
        if (size) {
            chunk.resize(size);
            buffer = SimpleMemoryBuffer::Create(std::move(chunk));
        }
        else {
            ok = 0 == errno;
            if (!ok) {
                MS_WARN_DEV_STD("Unable to read file chunk, size %ul, error code %d", chunk.size(), errno);
            }
        }
        if (ok) {
            eof = _fileSize == FileTell(handle);
        }
    }
    else {
        eof = true;
    }
    return buffer;
}

int64_t FileReader::GetFileSize(FILE* handle)
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

int64_t FileReader::FileTell(FILE* handle)
{
    if (handle) {
#ifdef _MSC_VER
        return ::_ftelli64(handle)
#else
        return ::ftell(handle);
#endif
    }
    return -1;
}

bool FileReader::FileSeek(FILE* handle, int command, long offset)
{
    if (handle && offset >= 0L) {
#ifdef _MSC_VER
        return 0 == ::_fseeki64(handle, offset, command);
#else
        ::fseek(handle, offset, command);
        return true;
#endif
    }
    return false;
}

FileReader::StartEndNotifier::StartEndNotifier(FileReader* source, bool restart)
    : _source(source)
{
    _source->StartMediaSinksWriting(restart);
}

FileReader::StartEndNotifier::~StartEndNotifier()
{
    _source->EndMediaSinksWriting();
}

} // namespace RTC
