#pragma once
#include "RTC/MediaTranslate/MediaFrameDeserializeResult.hpp"
#include <mkvparser/mkvreader.h>
#include <memory>

namespace RTC
{

class MemoryBuffer;

class MkvReader : public mkvparser::IMkvReader
{
public:
	virtual MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<MemoryBuffer>& buffer) = 0;
};

}
