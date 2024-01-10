#pragma once

#include "FBS/translationPack.h"
#include <atomic>
#include <string>
#include <optional>

namespace RTC
{

class TranslatorUnit
{
public:
    virtual ~TranslatorUnit() = default;
    virtual const std::string& GetId() const = 0;
    virtual std::optional<FBS::TranslationPack::Language> GetLanguage() const = 0;
    bool IsPaused() const { return _paused.load(std::memory_order_relaxed); }
    void Pause(bool pause = true);
    void Resume() { Pause(false); }
protected:
    virtual void OnPauseChanged(bool /*pause*/) {}
private:
    std::atomic_bool _paused = false;
};

}
