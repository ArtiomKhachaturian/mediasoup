#pragma once
#include "api/units/timestamp.h"
#include <mkvparser/mkvparser.h>

namespace RTC
{

enum class MkvReadResult;

class MkvEntry
{
public:
    MkvEntry() = default;
    MkvEntry(const mkvparser::BlockEntry* entry);
    MkvEntry(const MkvEntry&) = default;
    MkvEntry(MkvEntry&&) = default;
    bool IsValid() const { return nullptr != _entry; }
    bool IsEndOfStream() const;
    bool IsEndOfEntry() const;
    bool IsKey() const;
    const webrtc::Timestamp& GetTime() const { return _timestamp; }
    MkvReadResult ReadFirst(const mkvparser::Track* track);
    MkvReadResult ReadNext(const mkvparser::Track* track);
    // return current frame block and advance to the next
    mkvparser::Block::Frame NextFrame();
    explicit operator bool () const { return IsValid(); }
    MkvEntry& operator = (const mkvparser::BlockEntry* entry);
    MkvEntry& operator = (const MkvEntry&) = default;
    MkvEntry& operator = (MkvEntry&&) = default;
private:
    const mkvparser::Block* GetBlock() const;
    const mkvparser::Cluster* GetCluster() const;
    void SetEntry(const mkvparser::BlockEntry* entry);
    MkvReadResult SetValidEntry(long result, const mkvparser::BlockEntry* entry);
private:
    const mkvparser::BlockEntry* _entry = nullptr;
    webrtc::Timestamp _timestamp = webrtc::Timestamp::Zero();
    int _currentFrameIndex = 0;
};

} // namespace RTC
