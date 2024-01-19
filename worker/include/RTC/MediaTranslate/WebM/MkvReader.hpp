#pragma once
#include <mkvparser/mkvreader.h>
#include <memory>

namespace RTC
{

class MemoryBuffer;

class MkvReader : public mkvparser::IMkvReader
{
public:
	virtual bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) = 0;
};

}