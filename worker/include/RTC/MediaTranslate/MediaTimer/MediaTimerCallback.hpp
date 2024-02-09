#pragma once

namespace RTC
{

class MediaTimerCallback
{
public:
    virtual void OnEvent() = 0;
protected:
    virtual ~MediaTimerCallback() = default;
};

} // namespace RTC