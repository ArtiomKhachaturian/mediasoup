#ifndef MS_RTC_ACTIVE_SPEAKER_OBSERVER_HPP
#define MS_RTC_ACTIVE_SPEAKER_OBSERVER_HPP

#include "RTC/RtpObserver.hpp"
#include "RTC/Shared.hpp"
#include "handles/TimerHandle.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>
#include <utility>
#include <vector>

// Implementation of Dominant Speaker Identification for Multipoint
// Videoconferencing by Ilana Volfin and Israel Cohen. This implementation uses
// the RTP Audio Level extension from RFC-6464 for the input signal. This has
// been ported from DominantSpeakerIdentification.java in Jitsi:
// https://github.com/jitsi/jitsi-utils/blob/master/src/main/java/org/jitsi/utils/dsi/DominantSpeakerIdentification.java
namespace RTC
{
	class ActiveSpeakerObserver : public RTC::RtpObserver, public TimerHandle::Listener
	{
        class Speaker;
        class ProducerSpeaker;

	public:
		ActiveSpeakerObserver(
		  RTC::Shared* shared,
		  const std::string& id,
		  RTC::RtpObserver::Listener* listener,
		  const FBS::ActiveSpeakerObserver::ActiveSpeakerObserverOptions* options);
		~ActiveSpeakerObserver() override;

	public:
		void AddProducer(RTC::Producer* producer) override;
		void RemoveProducer(RTC::Producer* producer) override;
		void ReceiveRtpPacket(RTC::Producer* producer, RTC::RtpPacket* packet) override;
		void ProducerPaused(RTC::Producer* producer) override;
		void ProducerResumed(RTC::Producer* producer) override;

	private:
		void Paused() override;
		void Resumed() override;
		void Update();
		bool CalculateActiveSpeaker(const absl::flat_hash_map<std::string, ProducerSpeaker*>& mapProducerSpeakers);
		void TimeoutIdleLevels(uint64_t now);
        bool ResetDominantId(const std::string& previousDominantId);
        std::string GetDominantId() const;

		/* Pure virtual methods inherited from TimerHandle. */
	protected:
		void OnTimer(TimerHandle* timer) override;

	private:
        static inline constexpr size_t RelativeSpeachActivitiesLen = 3u;
        const std::unique_ptr<TimerHandle> periodicTimer;
        ProtectedObj<std::string> dominantId;
		// Map of ProducerSpeakers indexed by Producer id.
        ProtectedObj<absl::flat_hash_map<std::string, ProducerSpeaker*>> mapProducerSpeakers;
        ProtectedObj<uint64_t> lastLevelIdleTime{ 0u };
	};
} // namespace RTC

#endif
