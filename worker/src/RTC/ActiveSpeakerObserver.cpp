#define MS_CLASS "RTC::ActiveSpeakerObserver"

#include "RTC/ActiveSpeakerObserver.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <atomic>
#include <optional>

namespace RTC
{
	/* Static. */

	static constexpr uint32_t C1{ 3u };
	static constexpr uint32_t C2{ 2u };
	static constexpr uint32_t C3{ 0u };
	static constexpr uint32_t N1{ 13u };
	static constexpr uint32_t N2{ 5u };
	static constexpr uint32_t N3{ 10u };
	static constexpr uint32_t LongCount{ 1u };
	static constexpr uint32_t LevelIdleTimeout{ 40u };
	static constexpr uint64_t SpeakerIdleTimeout{ 60 * 60 * 1000 };
	static constexpr uint32_t LongThreashold{ 4u };
	static constexpr uint32_t MaxLevel{ 127u };
	static constexpr uint32_t MinLevel{ 0u };
	static constexpr uint32_t MinLevelWindowLen{ 15 * 1000 / 20 };
	static constexpr uint32_t MediumThreshold{ 7u };
	static constexpr uint32_t SubunitLengthN1{ (MaxLevel - MinLevel + N1 - 1) / N1 };
	static constexpr uint32_t ImmediateBuffLen{ LongCount * N3 * N2 };
	static constexpr uint32_t MediumsBuffLen{ LongCount * N3 };
	static constexpr uint32_t LongsBuffLen{ LongCount };
	static constexpr uint32_t LevelsBuffLen{ LongCount * N3 * N2 };
	static constexpr double MinActivityScore{ 0.0000000001 };

	inline int64_t BinomialCoefficient(int32_t n, int32_t r)
	{
		const int32_t m = n - r;

		if (r < m)
		{
			r = m;
		}

		int64_t t{ 1 };

		for (int64_t i = n, j = 1; i > r; i--, ++j)
		{
			t = t * i / j;
		}

		return t;
	}

	inline double ComputeActivityScore(
	  const uint8_t vL, const uint32_t nR, const double p, const double lambda)
	{
		double activityScore = std::log(BinomialCoefficient(nR, vL)) + vL * std::log(p) +
		                       (nR - vL) * std::log(1 - p) - std::log(lambda) + lambda * vL;

		if (activityScore < MinActivityScore)
		{
			activityScore = MinActivityScore;
		}

		return activityScore;
	}

	inline bool ComputeBigs(
	  const std::vector<uint8_t>& littles, std::vector<uint8_t>& bigs, uint8_t threashold)
	{
		const uint32_t littleLen       = littles.size();
		const uint32_t bigLen          = bigs.size();
		const uint32_t littleLenPerBig = littleLen / bigLen;
		bool changed{ false };

		for (uint32_t b = 0u, l = 0u; b < bigLen; ++b)
		{
			uint8_t sum{ 0u };

			for (const uint32_t lEnd = l + littleLenPerBig; l < lEnd; ++l)
			{
				if (littles[l] > threashold)
				{
					++sum;
				}
			}

			if (bigs[b] != sum)
			{
				bigs[b] = sum;
				changed = true;
			}
		}

		return changed;
	}

    class ActiveSpeakerObserver::Speaker
    {
        struct MinLevelEval
        {
            uint8_t level = MinLevel;
            uint8_t nextLevel = MinLevel;
            uint32_t nextLevelWindowLen{ 0u };
            double GetSqrt() const { return std::sqrt(static_cast<double>(this->level * this->nextLevel)); }
        };
    public:
        Speaker();
        void EvalActivityScores();
        double GetActivityScore(uint8_t interval) const;
        void LevelChanged(uint32_t level, uint64_t now);
        void LevelTimedOut(uint64_t now);
        uint64_t GetLastLevelChangeTime() const;
    private:
        bool ComputeImmediates();
        bool ComputeLongs();
        bool ComputeMediums();
        void EvalImmediateActivityScore();
        void EvalMediumActivityScore();
        void EvalLongActivityScore();
        void UpdateMinLevel(int8_t level);

    public:
        std::atomic_bool paused{ false };
        
    private:
        std::atomic<double> immediateActivityScore{ 0 };
        std::atomic<double> mediumActivityScore{ 0 };
        std::atomic<double> longActivityScore{ 0 };
        ProtectedObj<uint64_t> lastLevelChangeTime{ 0 };
        ProtectedObj<MinLevelEval> minLevel;
        ProtectedObj<std::vector<uint8_t>> immediates;
        ProtectedObj<std::vector<uint8_t>> mediums;
        ProtectedObj<std::vector<uint8_t>> longs;
        ProtectedObj<std::vector<uint8_t>> levels;
        size_t nextLevelIndex{ 0u }; // guarded with [levels]
    };

    class ActiveSpeakerObserver::ProducerSpeaker
    {
    public:
        explicit ProducerSpeaker(RTC::Producer* producer);
        ~ProducerSpeaker();

    public:
        RTC::Producer* const producer;
        Speaker* const speaker;
    };

	ActiveSpeakerObserver::ActiveSpeakerObserver(
	  RTC::Shared* shared,
	  const std::string& id,
	  RTC::RtpObserver::Listener* listener,
	  const FBS::ActiveSpeakerObserver::ActiveSpeakerObserverOptions* options)
	  : RTC::RtpObserver(shared, id, listener)
      , periodicTimer(new TimerHandle(this))
	{
		MS_TRACE();

        const auto interval = Utils::Bound<uint16_t>(100, options->interval(), 5000);
		this->periodicTimer->Start(interval, interval);

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelNotificationHandler*/ nullptr);
	}

	ActiveSpeakerObserver::~ActiveSpeakerObserver()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

        LOCK_WRITE_PROTECTED_OBJ(this->mapProducerSpeakers);
		// Must clear all entries in this->mapProducerSpeakers since RemoveProducer()
		// won't be called for each existing Producer if the ActiveSpeakerObserver or
		// its parent Router are directly closed.
		for (auto& kv : this->mapProducerSpeakers.Ref())
		{
			auto* producerSpeaker = kv.second;
			delete producerSpeaker;
		}
		this->mapProducerSpeakers->clear();
	}

	void ActiveSpeakerObserver::AddProducer(RTC::Producer* producer)
	{
		MS_TRACE();

		if (producer->GetKind() != RTC::Media::Kind::AUDIO)
		{
			MS_THROW_TYPE_ERROR("not an audio Producer");
		}
        
        LOCK_WRITE_PROTECTED_OBJ(this->mapProducerSpeakers);

		if (this->mapProducerSpeakers->find(producer->id) != this->mapProducerSpeakers->end())
		{
			MS_THROW_ERROR("Producer already in map");
		}
        
        this->mapProducerSpeakers->insert(std::make_pair(producer->id, new ProducerSpeaker(producer)));

	}

	void ActiveSpeakerObserver::RemoveProducer(RTC::Producer* producer)
	{
		MS_TRACE();
        
        LOCK_WRITE_PROTECTED_OBJ(this->mapProducerSpeakers);

		auto it = this->mapProducerSpeakers->find(producer->id);

		if (it == this->mapProducerSpeakers->end())
		{
			return;
		}

		auto* producerSpeaker = it->second;

		delete producerSpeaker;

		this->mapProducerSpeakers->erase(producer->id);
        
        if (ResetDominantId(producer->id)) {

            Update();
        }
	}

	void ActiveSpeakerObserver::ProducerPaused(RTC::Producer* producer)
	{
		MS_TRACE();
        
        LOCK_READ_PROTECTED_OBJ(this->mapProducerSpeakers);

		auto it = this->mapProducerSpeakers->find(producer->id);

		if (it != this->mapProducerSpeakers->end())
		{
			auto* producerSpeaker = it->second;

			producerSpeaker->speaker->paused = true;
		}
	}

	void ActiveSpeakerObserver::ProducerResumed(RTC::Producer* producer)
	{
		MS_TRACE();
        
        LOCK_READ_PROTECTED_OBJ(this->mapProducerSpeakers);

		auto it = this->mapProducerSpeakers->find(producer->id);

		if (it != this->mapProducerSpeakers->end())
		{
			auto* producerSpeaker = it->second;

			producerSpeaker->speaker->paused = false;
		}
	}

	void ActiveSpeakerObserver::ReceiveRtpPacket(RTC::Producer* producer, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		if (IsPaused())
		{
			return;
		}

		uint8_t level;
		bool voice;

		if (!packet->ReadSsrcAudioLevel(level, voice))
		{
			return;
		}

		const uint8_t volume = 127 - level;
        
        LOCK_READ_PROTECTED_OBJ(this->mapProducerSpeakers);

		auto it = this->mapProducerSpeakers->find(producer->id);

		if (it != this->mapProducerSpeakers->end())
		{
			auto* producerSpeaker = it->second;
			const uint64_t now    = DepLibUV::GetTimeMs();

			producerSpeaker->speaker->LevelChanged(volume, now);
		}
	}

	void ActiveSpeakerObserver::Paused()
	{
		MS_TRACE();

		this->periodicTimer->Stop();
	}

	void ActiveSpeakerObserver::Resumed()
	{
		MS_TRACE();

		this->periodicTimer->Restart();
	}

	void ActiveSpeakerObserver::OnTimer(TimerHandle* /*timer*/)
	{
		MS_TRACE();

		Update();
	}

	void ActiveSpeakerObserver::Update()
	{
		MS_TRACE();

		const uint64_t now = DepLibUV::GetTimeMs();

        {
            LOCK_WRITE_PROTECTED_OBJ(this->lastLevelIdleTime);
            if (now - this->lastLevelIdleTime.ConstRef() >= LevelIdleTimeout)
            {
                if (this->lastLevelIdleTime.ConstRef() != 0)
                {
                    TimeoutIdleLevels(now);
                }
                
                this->lastLevelIdleTime = now;
            }
        }
        
        LOCK_READ_PROTECTED_OBJ(this->mapProducerSpeakers);

		if (!this->mapProducerSpeakers->empty() && CalculateActiveSpeaker(this->mapProducerSpeakers))
		{
			auto notification = FBS::ActiveSpeakerObserver::CreateDominantSpeakerNotificationDirect(
			  this->shared->channelNotifier->GetBufferBuilder(), GetDominantId().c_str());

			this->shared->channelNotifier->Emit(
			  this->id,
			  FBS::Notification::Event::ACTIVESPEAKEROBSERVER_DOMINANT_SPEAKER,
			  FBS::Notification::Body::ActiveSpeakerObserver_DominantSpeakerNotification,
			  notification);
		}
	}

	bool ActiveSpeakerObserver::CalculateActiveSpeaker(const absl::flat_hash_map<std::string, ProducerSpeaker*>& mapProducerSpeakers)
	{
		MS_TRACE();

		std::string newDominantId;
		const int32_t speakerCount = mapProducerSpeakers.size();

		if (speakerCount == 0)
		{
			newDominantId = "";
		}
		else if (speakerCount == 1)
		{
			auto it               = mapProducerSpeakers.begin();
			auto* producerSpeaker = it->second;
			newDominantId         = producerSpeaker->producer->id;
		}
		else
		{
            LOCK_WRITE_PROTECTED_OBJ(this->dominantId);
			Speaker* dominantSpeaker = (this->dominantId->empty())
			                             ? nullptr
			                             : mapProducerSpeakers.at(this->dominantId.ConstRef())->speaker;

			if (dominantSpeaker == nullptr)
			{
				auto it               = mapProducerSpeakers.begin();
				newDominantId         = it->first;
				auto* producerSpeaker = it->second;
				dominantSpeaker       = producerSpeaker->speaker;
			}
			else
			{
				newDominantId = "";
			}

			dominantSpeaker->EvalActivityScores();
			double newDominantC2 = C2;

			for (const auto& kv : mapProducerSpeakers)
			{
				auto* producerSpeaker = kv.second;
				auto* speaker         = producerSpeaker->speaker;
				const auto& id        = producerSpeaker->producer->id;

				if (id == this->dominantId.ConstRef() || speaker->paused)
				{
					continue;
				}

				speaker->EvalActivityScores();
                
                static thread_local double relativeSpeachActivities[RelativeSpeachActivitiesLen] = {};

				for (uint8_t interval = 0u; interval < ActiveSpeakerObserver::RelativeSpeachActivitiesLen;
				     ++interval)
				{
					relativeSpeachActivities[interval] = std::log(
					  speaker->GetActivityScore(interval) / dominantSpeaker->GetActivityScore(interval));
				}

				const double c1 = relativeSpeachActivities[0];
				const double c2 = relativeSpeachActivities[1];
				const double c3 = relativeSpeachActivities[2];

				if ((c1 > C1) && (c2 > C2) && (c3 > C3) && (c2 > newDominantC2))
				{
					newDominantC2 = c2;
					newDominantId = id;
				}
			}
		}

		if (!newDominantId.empty() && newDominantId != this->dominantId.ConstRef())
		{
			this->dominantId = std::move(newDominantId);

			return true;
		}

		return false;
	}

	void ActiveSpeakerObserver::TimeoutIdleLevels(uint64_t now)
	{
		MS_TRACE();
        
        LOCK_READ_PROTECTED_OBJ(this->mapProducerSpeakers);

		for (const auto& kv : this->mapProducerSpeakers.ConstRef())
		{
			auto* producerSpeaker = kv.second;
			auto* speaker         = producerSpeaker->speaker;
			const auto& id        = producerSpeaker->producer->id;
			const uint64_t idle   = now - speaker->GetLastLevelChangeTime();
            const auto dominantId = GetDominantId();

			if (SpeakerIdleTimeout < idle && (dominantId.empty() || id != dominantId))
			{
				speaker->paused = true;
			}
			else if (LevelIdleTimeout < idle)
			{
				speaker->LevelTimedOut(now);
			}
		}
	}

    bool ActiveSpeakerObserver::ResetDominantId(const std::string& previousDominantId)
    {
        if (!previousDominantId.empty()) {
            LOCK_WRITE_PROTECTED_OBJ(this->dominantId);
            if (this->dominantId.ConstRef() == previousDominantId) {
                this->dominantId->clear();
                return true;
            }
        }
        return false;
    }

    std::string ActiveSpeakerObserver::GetDominantId() const
    {
        LOCK_READ_PROTECTED_OBJ(this->dominantId);
        return this->dominantId;
    }

	ActiveSpeakerObserver::ProducerSpeaker::ProducerSpeaker(RTC::Producer* producer)
	  : producer(producer), speaker(new Speaker())
	{
		MS_TRACE();

		this->speaker->paused = producer->IsPaused();
	}

	ActiveSpeakerObserver::ProducerSpeaker::~ProducerSpeaker()
	{
		MS_TRACE();

		delete this->speaker;
	}

	ActiveSpeakerObserver::Speaker::Speaker()
	  : immediateActivityScore(MinActivityScore), mediumActivityScore(MinActivityScore),
	    longActivityScore(MinActivityScore), lastLevelChangeTime(DepLibUV::GetTimeMs()),
	    immediates(ImmediateBuffLen, 0),
	    mediums(MediumsBuffLen, 0), longs(LongsBuffLen, 0), levels(LevelsBuffLen, 0)
	{
		MS_TRACE();
	}

	void ActiveSpeakerObserver::Speaker::EvalActivityScores()
	{
		MS_TRACE();

		if (ComputeImmediates())
		{
			EvalImmediateActivityScore();

			if (ComputeMediums())
			{
				EvalMediumActivityScore();

				if (ComputeLongs())
				{
					EvalLongActivityScore();
				}
			}
		}
	}

	double ActiveSpeakerObserver::Speaker::GetActivityScore(uint8_t interval) const
	{
		MS_TRACE();

		switch (interval)
		{
			case 0u:
				return this->immediateActivityScore;
			case 1u:
				return this->mediumActivityScore;
			case 2u:
				return this->longActivityScore;
			default:
				MS_ABORT("interval is invalid");
		}

		return 0;
	}

	void ActiveSpeakerObserver::Speaker::LevelChanged(uint32_t level, uint64_t now)
	{
        std::optional<uint64_t> elapsed;
        {
            LOCK_WRITE_PROTECTED_OBJ(this->lastLevelChangeTime);
            if (this->lastLevelChangeTime <= now) {
                elapsed = now - this->lastLevelChangeTime;
                this->lastLevelChangeTime = now;
            }
        }
		if (elapsed.has_value())
		{
			int8_t b{ 0 };

			if (level < MinLevel)
			{
				b = MinLevel;
			}
			else if (level > MaxLevel)
			{
				b = MaxLevel;
			}
			else
			{
				b = level;
			}

			// The algorithm expect to have an update every 20 milliseconds. If the
			// Producer is paused, using a different packetization time or using DTX
			// we need to update more than one sample when receiving an audio packet.
			const uint32_t intervalsUpdated =
			  std::min(std::max(static_cast<uint32_t>(elapsed.value() / 20), 1U), LevelsBuffLen);

            {
                LOCK_WRITE_PROTECTED_OBJ(this->levels);
                auto& levels = this->levels.Ref();
                
                for (uint32_t i{ 0u }; i < intervalsUpdated; ++i)
                {
                    levels[this->nextLevelIndex] = b;
                    this->nextLevelIndex         = (this->nextLevelIndex + 1) % LevelsBuffLen;
                }
            }

			UpdateMinLevel(b);
		}
	}

	void ActiveSpeakerObserver::Speaker::LevelTimedOut(uint64_t now)
	{
		MS_TRACE();

		LevelChanged(MinLevel, now);
	}

    uint64_t ActiveSpeakerObserver::Speaker::GetLastLevelChangeTime() const
    {
        LOCK_READ_PROTECTED_OBJ(this->lastLevelChangeTime);
        return this->lastLevelChangeTime.ConstRef();
    }

	bool ActiveSpeakerObserver::Speaker::ComputeImmediates()
	{
		MS_TRACE();
        
        int8_t minLevel = 0;
        
        {
            LOCK_READ_PROTECTED_OBJ(this->minLevel);
            minLevel = this->minLevel->level + SubunitLengthN1;
        }

        bool changed          = false;
        
        LOCK_READ_PROTECTED_OBJ(this->levels);

		for (uint32_t i = 0; i < ImmediateBuffLen; ++i)
		{
			// this->levels is a circular buffer where new samples are written in the
			// next vector index. this->immediates is a buffer where the most recent
			// value is always in index 0.
			const size_t levelIndex = this->nextLevelIndex >= (i + 1)
			                            ? this->nextLevelIndex - i - 1
			                            : this->nextLevelIndex + LevelsBuffLen - i - 1;
			uint8_t level           = this->levels->at(levelIndex);

			if (level < minLevel)
			{
				level = MinLevel;
			}

			const uint8_t immediate = (level / SubunitLengthN1);
            
            LOCK_WRITE_PROTECTED_OBJ(this->immediates);
            auto& immediates = this->immediates.Ref();

			if (immediates[i] != immediate)
			{
                immediates[i] = immediate;
				changed       = true;
			}
		}

		return changed;
	}

	bool ActiveSpeakerObserver::Speaker::ComputeMediums()
	{
		MS_TRACE();
        
        LOCK_READ_PROTECTED_OBJ(this->immediates);
        LOCK_WRITE_PROTECTED_OBJ(this->mediums);

		return ComputeBigs(this->immediates.ConstRef(), this->mediums.Ref(), MediumThreshold);
	}

	bool ActiveSpeakerObserver::Speaker::ComputeLongs()
	{
		MS_TRACE();
        
        LOCK_READ_PROTECTED_OBJ(this->mediums);
        LOCK_WRITE_PROTECTED_OBJ(this->longs);

		return ComputeBigs(this->mediums.ConstRef(), this->longs.Ref(), LongThreashold);
	}

	void ActiveSpeakerObserver::Speaker::EvalImmediateActivityScore()
	{
		MS_TRACE();
        
        LOCK_READ_PROTECTED_OBJ(this->immediates);

		this->immediateActivityScore = ComputeActivityScore(this->immediates->at(0), N1, 0.5, 0.78);
	}

	void ActiveSpeakerObserver::Speaker::EvalMediumActivityScore()
	{
		MS_TRACE();
        
        LOCK_READ_PROTECTED_OBJ(this->mediums);
		
        this->mediumActivityScore = ComputeActivityScore(this->mediums->at(0), N2, 0.5, 24);
	}

	void ActiveSpeakerObserver::Speaker::EvalLongActivityScore()
	{
		MS_TRACE();
        
        LOCK_READ_PROTECTED_OBJ(this->longs);

		this->longActivityScore = ComputeActivityScore(this->longs->at(0), N3, 0.5, 47);
	}

	void ActiveSpeakerObserver::Speaker::UpdateMinLevel(int8_t level)
	{
		MS_TRACE();

		if (level == MinLevel)
		{
			return;
		}
        
        LOCK_WRITE_PROTECTED_OBJ(this->minLevel);

		if ((this->minLevel->level == MinLevel) || (this->minLevel->level > level))
		{
			this->minLevel->level              = level;
			this->minLevel->nextLevel          = MinLevel;
			this->minLevel->nextLevelWindowLen = 0;
		}
		else
		{
			if (this->minLevel->nextLevel == MinLevel)
			{
                this->minLevel->nextLevel          = level;
				this->minLevel->nextLevelWindowLen = 1;
			}
			else
			{
				if (this->minLevel->nextLevel > level)
				{
                    this->minLevel->nextLevel = level;
				}

                this->minLevel->nextLevelWindowLen++;

				if (this->minLevel->nextLevelWindowLen >= MinLevelWindowLen)
				{
					double newMinLevel = this->minLevel->GetSqrt();

					if (newMinLevel < MinLevel)
					{
						newMinLevel = MinLevel;
					}
					else if (newMinLevel > MaxLevel)
					{
						newMinLevel = MaxLevel;
					}

                    this->minLevel->level              = static_cast<int8_t>(newMinLevel);
                    this->minLevel->nextLevel          = MinLevel;
                    this->minLevel->nextLevelWindowLen = 0;
				}
			}
		}
	}
} // namespace RTC
