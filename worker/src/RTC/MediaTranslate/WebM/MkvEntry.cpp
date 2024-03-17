#define MS_CLASS "RTC::MkvEntry"
#include "RTC/MediaTranslate/WebM/MkvEntry.hpp"
#include "RTC/MediaTranslate/WebM/MkvReadResult.hpp"
#include "Logger.hpp"

namespace RTC
{

MkvEntry::MkvEntry(const mkvparser::BlockEntry* entry)
{
    SetEntry(entry);
}

bool MkvEntry::IsEndOfStream() const 
{ 
	return _entry && _entry->EOS(); 
}

bool MkvEntry::IsEndOfEntry() const
{
    if (const auto block = GetBlock()) {
        return _currentFrameIndex == block->GetFrameCount();
    }
    return true;
}

bool MkvEntry::IsKey() const
{
    const auto block = GetBlock();
    return block && block->IsKey();
}

MkvReadResult MkvEntry::ReadFirst(const mkvparser::Track* track)
{
    if (track) {
        const mkvparser::BlockEntry* entry = nullptr;
        const auto res = track->GetFirst(entry);
        return SetValidEntry(res, entry);
    }
    return MkvReadResult::InvalidInputArg;
}

MkvReadResult MkvEntry::ReadNext(const mkvparser::Track* track)
{
    if (track) {
        if (_entry) {
            if (!_entry->EOS()) {
                const mkvparser::BlockEntry* nextEntry = nullptr;
                const auto res = track->GetNext(_entry, nextEntry);
                return SetValidEntry(res, nextEntry);
            }
            return MkvReadResult::NoMoreClusters;
        }
        return MkvReadResult::UnknownError;
    }
    return MkvReadResult::InvalidInputArg;
}

mkvparser::Block::Frame MkvEntry::NextFrame()
{
    if (const auto block = GetBlock()) {
        if (_currentFrameIndex < block->GetFrameCount()) {
            return block->GetFrame(_currentFrameIndex++);
        }
    }
    return {0, 0};
}

MkvEntry& MkvEntry::operator = (const mkvparser::BlockEntry* entry)
{
    SetEntry(entry);
    return *this;
}

const mkvparser::Block* MkvEntry::GetBlock() const
{
    return _entry ? _entry->GetBlock() : nullptr;
}

const mkvparser::Cluster* MkvEntry::GetCluster() const
{
    return _entry ? _entry->GetCluster() : nullptr;
}

void MkvEntry::SetEntry(const mkvparser::BlockEntry* entry)
{
    if (entry != _entry) {
        _entry = entry;
        _currentFrameIndex = 0;
        if (_entry) {
            const auto block = _entry->GetBlock();
            MS_ASSERT(block, "invalid block of entry");
            _timestamp = webrtc::Timestamp::us(block->GetTime(GetCluster()) / 1000);
        }
        else {
        	_timestamp = webrtc::Timestamp::Zero();
        }
    }
}

MkvReadResult MkvEntry::SetValidEntry(long result, const mkvparser::BlockEntry* entry)
{
    if (0 == result) {
        MS_ASSERT(entry, "invalid entry");
        SetEntry(entry);
        if (entry->EOS()) {
            return MkvReadResult::NoMoreClusters;
        }
    }
    return ToMkvReadResult(result);
}

} // namespace RTC
