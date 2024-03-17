#define MS_CLASS "RTC::FileSinkWriter"
#include "RTC/MediaTranslate/FileSinkWriter.hpp"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/Buffers/Buffer.hpp"
#include "Logger.hpp"

namespace RTC
{

FileSinkWriter::FileSinkWriter(std::string fileName, FileWriter file)
    : _fileName(std::move(fileName))
    , _file(std::move(file))
{
}

std::unique_ptr<FileSinkWriter> FileSinkWriter::Create(std::string fileName)
{
    std::unique_ptr<FileSinkWriter> sink;
    if (!fileName.empty()) {
        FileWriter file;
        if (file.Open(fileName)) {
            sink.reset(new FileSinkWriter(std::move(fileName), std::move(file)));
        }
        else {
            MS_ERROR("failed to open file output '%s', error: %s", fileName.c_str(),
                     file.GetError().message().c_str());
        }
    }
    return sink;
}

void FileSinkWriter::DeleteFromStorage()
{
    if (Close() && FileReader::IsReadable(GetFileName())) {
        const auto res = FileWriter::DeleteFromStorage(GetFileName());
        if (res) {
            MS_ERROR("failed to delete file '%s', error: %s", GetFileName(),
                     res.message().c_str());
        }
    }
}

void FileSinkWriter::StartMediaWriting(uint64_t senderId)
{
    MediaSink::StartMediaWriting(senderId);
    LOCK_WRITE_PROTECTED_OBJ(_file);
    if (_file->IsOpen() && !_file->Truncate()) {
        MS_ERROR("failed to truncate file '%s', error: %s",
                 GetFileName(), _file->GetError().message().c_str());
    }
}

void FileSinkWriter::WriteMediaPayload(uint64_t, const std::shared_ptr<Buffer>& buffer)
{
    if (buffer && !buffer->IsEmpty()) {
        LOCK_WRITE_PROTECTED_OBJ(_file);
        if (_file->IsOpen()) {
            const auto expected = buffer->GetSize();
            const auto actual = _file->Write(buffer);
            if (expected != actual) {
                MS_ERROR("file '%s' write error, expected %zu but "
                         "written only %zu bytes, error: %s",
                         GetFileName(), expected, actual,
                         _file->GetError().message().c_str());
                _file->Close();
            }
        }
    }
}

void FileSinkWriter::EndMediaWriting(uint64_t senderId)
{
    MediaSink::EndMediaWriting(senderId);
    LOCK_WRITE_PROTECTED_OBJ(_file);
    if (_file->IsOpen() && !_file->Flush()) {
        MS_ERROR("failed to flush file '%s' data, error: %s",
                 GetFileName(), _file->GetError().message().c_str());
    }
}

bool FileSinkWriter::Close()
{
    bool ok = false;
    LOCK_WRITE_PROTECTED_OBJ(_file);
    if (_file->IsOpen()) {
        ok = _file->Close();
        if (!ok) {
            MS_ERROR("failed to close file '%s', error: %s",
                     GetFileName(), _file->GetError().message().c_str());
        }
    }
    else {
        ok = true; // already closed
    }
    return ok;
}

} // namespace RTC
