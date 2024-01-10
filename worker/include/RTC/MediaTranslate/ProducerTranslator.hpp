#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/TranslatorUnit.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include "RTC/MediaTranslate/ProducerObserver.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/TranslatorUnit.hpp"
#include "RTC/Listeners.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class ProducerObserver;
class RtpStream;
class Producer;

class ProducerTranslator : public TranslatorUnit,
                           public ProducerInputMediaStreamer,
                           public RtpPacketsCollector,
                           private OutputDevice
{
    class StreamInfo;
public:
    ProducerTranslator(const Producer* producer);
    ~ProducerTranslator() final;
    bool IsAudio() const;
    void AddObserver(ProducerObserver* observer);
    void RemoveObserver(ProducerObserver* observer);
    bool AddStream(const RtpStream* stream, uint32_t mappedSsrc);
    bool AddStream(const RtpCodecMimeType& mime, uint32_t clockRate, uint32_t mappedSsrc);
    bool RemoveStream(uint32_t mappedSsrc);
    std::string_view GetFileExtension(uint32_t mappedSsrc) const;
    // list of ssrcs
    std::list<uint32_t> GetAddedStreams() const;
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of RtpPacketsCollector
    void AddPacket(const RtpPacket* packet) final;
    // impl. of TranslatorUnit
    std::optional<FBS::TranslationPack::Language> GetLanguage() const final;
    // impl. of ProducerInputMediaStreamer
    void AddOutputDevice(OutputDevice* outputDevice) final;
    void RemoveOutputDevice(OutputDevice* outputDevice) final;
protected:
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
    // override of TranslatorUnit
    void OnPauseChanged(bool pause) final;
private:
    // impl. of OutputDevice
    void StartStream(bool restart) noexcept final;
    void BeginWriteMediaPayload(uint32_t ssrc,
                                const std::vector<RtpMediaPacketInfo>& packets) noexcept final;
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept final;
    void EndWriteMediaPayload(uint32_t ssrc, const std::vector<RtpMediaPacketInfo>& packets,
                              bool ok) noexcept final;
    void EndStream(bool failure) noexcept final;
private:
    const Producer* const _producer;
    Listeners<ProducerObserver*> _observers;
    Listeners<OutputDevice*> _outputDevices;
    // key is mapped media SSRC
    absl::flat_hash_map<uint32_t, std::unique_ptr<StreamInfo>> _streams;
};

} // namespace RTC
