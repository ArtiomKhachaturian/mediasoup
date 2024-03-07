#define MS_CLASS "RTC::AudioLevelObserver"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/AudioLevelObserver.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <cmath> // std::lround()

namespace RTC
{
	/* Instance methods. */

    int8_t AudioLevelObserver::DBovs::GetAvg() const
    {
        return static_cast<int8_t>(std::lround(float(this->totalSum.load()) / this->count.load()));
    }

	AudioLevelObserver::AudioLevelObserver(
	  RTC::Shared* shared,
	  const std::string& id,
	  RTC::RtpObserver::Listener* listener,
	  const FBS::AudioLevelObserver::AudioLevelObserverOptions* options)
	  : RTC::RtpObserver(shared, id, listener),
        maxEntries(options->maxEntries()),
        threshold(options->threshold()),
        periodicTimer(new TimerHandle(this))
	{
		MS_TRACE();

		if (this->threshold > 0)
		{
			MS_THROW_TYPE_ERROR("invalid threshold value %" PRIi8, this->threshold);
		}

        const auto interval = Utils::Bound<uint16_t>(250, options->interval(), 5000);
		this->periodicTimer->Start(interval, interval);

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelNotificationHandler*/ nullptr);
	}

	AudioLevelObserver::~AudioLevelObserver()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

	}

	void AudioLevelObserver::AddProducer(RTC::Producer* producer)
	{
		MS_TRACE();

		if (producer->GetKind() != RTC::Media::Kind::AUDIO)
		{
			MS_THROW_TYPE_ERROR("not an audio Producer");
		}

		// Insert into the map.
        LOCK_WRITE_PROTECTED_OBJ(this->mapProducerDBovs);
        this->mapProducerDBovs->emplace(producer, std::make_unique<DBovs>());
	}

	void AudioLevelObserver::RemoveProducer(RTC::Producer* producer)
	{
		MS_TRACE();
        LOCK_WRITE_PROTECTED_OBJ(this->mapProducerDBovs);
		// Remove from the map.
        this->mapProducerDBovs->erase(producer);
	}

	void AudioLevelObserver::ReceiveRtpPacket(RTC::Producer* producer, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		if (IsPaused())
		{
			return;
		}

		uint8_t volume;
		bool voice;

		if (!packet->ReadSsrcAudioLevel(volume, voice))
		{
			return;
		}
        
        LOCK_READ_PROTECTED_OBJ(this->mapProducerDBovs);
		const auto& dBovs = this->mapProducerDBovs->at(producer);

		dBovs->totalSum.fetch_add(volume);
		dBovs->count.fetch_add(1U);
	}

	void AudioLevelObserver::ProducerPaused(RTC::Producer* producer)
	{
		// Remove from the map.
        LOCK_WRITE_PROTECTED_OBJ(this->mapProducerDBovs);
		this->mapProducerDBovs->erase(producer);
	}

	void AudioLevelObserver::ProducerResumed(RTC::Producer* producer)
	{
		// Insert into the map.
        LOCK_WRITE_PROTECTED_OBJ(this->mapProducerDBovs);
        this->mapProducerDBovs->emplace(producer, std::make_unique<DBovs>());
	}

	void AudioLevelObserver::Paused()
	{
		MS_TRACE();

		this->periodicTimer->Stop();

		ResetMapProducerDBovs();

		if (!this->silence.exchange(true))
		{
			this->shared->channelNotifier->Emit(
			  this->id, FBS::Notification::Event::AUDIOLEVELOBSERVER_SILENCE);
		}
	}

	void AudioLevelObserver::Resumed()
	{
		MS_TRACE();

		this->periodicTimer->Restart();
	}

	void AudioLevelObserver::Update()
	{
		MS_TRACE();

		absl::btree_multimap<int8_t, RTC::Producer*> mapDBovsProducer;
        
        {
            LOCK_READ_PROTECTED_OBJ(this->mapProducerDBovs);
            
            for (const auto& kv : this->mapProducerDBovs.ConstRef())
            {
                auto* producer = kv.first;
                const auto& dBovs    = kv.second;
                
                if (dBovs->count < 10)
                {
                    continue;
                }
                
                auto avgDBov = -1 * dBovs->GetAvg();
                
                if (avgDBov >= this->threshold)
                {
                    mapDBovsProducer.insert({ avgDBov, producer });
                }
            }
        }

		// Clear the map.
		ResetMapProducerDBovs();

		if (!mapDBovsProducer.empty())
		{
			this->silence = false;

			uint16_t idx{ 0 };
			auto rit = mapDBovsProducer.crbegin();

			std::vector<flatbuffers::Offset<FBS::AudioLevelObserver::Volume>> volumes;

			for (; idx < this->maxEntries && rit != mapDBovsProducer.crend(); ++idx, ++rit)
			{
				volumes.emplace_back(FBS::AudioLevelObserver::CreateVolumeDirect(
				  this->shared->channelNotifier->GetBufferBuilder(), rit->second->id.c_str(), rit->first));
			}

			auto notification = FBS::AudioLevelObserver::CreateVolumesNotificationDirect(
			  this->shared->channelNotifier->GetBufferBuilder(), &volumes);

			this->shared->channelNotifier->Emit(
			  this->id,
			  FBS::Notification::Event::AUDIOLEVELOBSERVER_VOLUMES,
			  FBS::Notification::Body::AudioLevelObserver_VolumesNotification,
			  notification);
		}
		else if (!this->silence.exchange(true))
		{
			this->shared->channelNotifier->Emit(
			  this->id, FBS::Notification::Event::AUDIOLEVELOBSERVER_SILENCE);
		}
	}

	void AudioLevelObserver::ResetMapProducerDBovs()
	{
		MS_TRACE();

        LOCK_READ_PROTECTED_OBJ(this->mapProducerDBovs);
        
		for (const auto& kv : this->mapProducerDBovs.ConstRef())
		{
			const auto& dBovs = kv.second;

			dBovs->totalSum = 0;
			dBovs->count    = 0;
		}
	}

	inline void AudioLevelObserver::OnTimer(TimerHandle* /*timer*/)
	{
		MS_TRACE();

		Update();
	}
} // namespace RTC
