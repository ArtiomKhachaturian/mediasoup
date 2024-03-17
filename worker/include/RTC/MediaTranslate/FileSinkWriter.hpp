#pragma once
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "ProtectedObj.hpp"

namespace RTC
{

class FileSinkWriter : public MediaSink
{
public:
    ~FileSinkWriter() final { Close(); }
    static std::unique_ptr<FileSinkWriter> Create(std::string fileName);
    void DeleteFromStorage();
    // impl. of MediaSink
    void StartMediaWriting(uint64_t senderId) final;
    void WriteMediaPayload(uint64_t senderId, const std::shared_ptr<Buffer>& buffer) final;
    void EndMediaWriting(uint64_t senderId) final;
private:
    FileSinkWriter(std::string fileName, FileWriter file);
    // return true if file was opened
    bool Close();
    const char* GetFileName() const { return _fileName.c_str(); }
private:
    const std::string _fileName;
    ProtectedObj<FileWriter, std::mutex> _file;
};

} // namespace RTC
