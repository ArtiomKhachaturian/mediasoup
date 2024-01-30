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
    : FileDevice<MediaSource>(fileNameUtf8, true, error)
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
    return FileDevice<MediaSource>::IsOpen() && _fileSize > 0LL && _chunkSize > 0UL;
}

void FileReader::OnFirstSinkAdded()
{
    FileDevice<MediaSource>::OnFirstSinkAdded();
    if (IsOpen() && !_thread.joinable()) {
        _thread = std::thread([this]() {
            if (!IsStopRequested()) {
                bool operationDone = false;
                for(; (!operationDone || _loop) && !IsStopRequested();) {
                    if (StartRead(operationDone)) {
                        operationDone = true;
                        if (_loop) { // seek to start
                            if (!FileSeek(GetHandle(), SEEK_SET, 0L)) {
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

void FileReader::OnLastSinkRemoved()
{
    Stop();
    FileDevice<MediaSource>::OnLastSinkRemoved();
}

void FileReader::Stop()
{
    if (_thread.joinable() && !_stopRequested.exchange(true)) {
        _thread.join();
        _stopRequested = false;
    }
}

bool FileReader::StartRead(bool restart)
{
    if (!IsStopRequested()) {
        const StartEndNotifier notifier(this, restart);
        bool eof = false, error = false;
        while (!IsStopRequested() && !eof && !error) {
            if (const auto buffer = ReadBuffer(eof, &error)) {
                WriteMediaSinksPayload(_ssrc.load(), buffer);
            }
        }
        if (error) {
            return false;
        }
    }
    return !IsStopRequested();
}

std::shared_ptr<const MemoryBuffer> FileReader::ReadBuffer(bool& eof, bool* error) const
{
    std::shared_ptr<const MemoryBuffer> buffer;
    if (const auto handle = GetHandle()) {
        bool ok = true;
        std::vector<uint8_t> chunk;
        chunk.resize(_chunkSize, 0);
        const auto size = ::fread(chunk.data(), 1UL, chunk.size(), handle);
        if (size) {
            chunk.resize(size);
            buffer = SimpleMemoryBuffer::Create(std::move(chunk));
            const auto currentPos = FileTell(handle);
            ok = currentPos >= 0LL && FileSeek(handle, SEEK_SET, currentPos + size);
        }
        else {
            ok = errno != 0;
        }
        if (ok) {
            eof = size < _chunkSize;
        }
        if (error) {
            *error = !ok;
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
