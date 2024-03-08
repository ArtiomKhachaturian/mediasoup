#define MS_CLASS "RTC::TransportCongestionControlClient"
// #define MS_LOG_DEV_LEVEL 3
#include "RTC/TransportCongestionControlClient.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include <libwebrtc/api/transport/network_types.h> // webrtc::TargetRateConstraints
#include <limits>

namespace RTC
{
	/* Static. */

	// NOTE: TransportCongestionControlMinOutgoingBitrate is defined in
	// TransportCongestionControlClient.hpp and exposed publicly.
	static constexpr float MaxBitrateMarginFactor{ 0.1f };
	static constexpr float MaxBitrateIncrementFactor{ 1.35f };
	static constexpr float MaxPaddingBitrateFactor{ 0.85f };
	static constexpr uint64_t AvailableBitrateEventInterval{ 1000u }; // In ms.
	static constexpr size_t PacketLossHistogramLength{ 24 };

	/* Instance methods. */

	TransportCongestionControlClient::TransportCongestionControlClient(
	  RTC::TransportCongestionControlClient::Listener* listener,
	  RTC::BweType bweType,
	  uint32_t initialAvailableBitrate,
	  uint32_t maxOutgoingBitrate,
	  uint32_t minOutgoingBitrate)
	  : listener(listener), bweType(bweType),
	    initialAvailableBitrate(std::max<uint32_t>(
	      initialAvailableBitrate, RTC::TransportCongestionControlMinOutgoingBitrate)),
	    maxOutgoingBitrate(maxOutgoingBitrate), minOutgoingBitrate(minOutgoingBitrate),
        controllerFactory(CreateControllerFactory())
	{
		MS_TRACE();
	}

	TransportCongestionControlClient::~TransportCongestionControlClient()
	{
		MS_TRACE();

        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        this->rtpTransportControllerSend->reset();
        this->probationGenerator.reset();
        processTimer.reset();
	}

    std::unique_ptr<webrtc::NetworkControllerFactoryInterface> TransportCongestionControlClient::CreateControllerFactory()
    {
        webrtc::GoogCcFactoryConfig config;
        // Provide RTCP feedback as well as Receiver Reports.
        config.feedback_only = true;
        return std::make_unique<webrtc::GoogCcNetworkControllerFactory>(std::move(config));
    }

	void TransportCongestionControlClient::TransportConnected()
	{
		MS_TRACE();
        
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (!this->rtpTransportControllerSend.ConstRef()) {
            webrtc::BitrateConstraints bitrateConfig;
            bitrateConfig.start_bitrate_bps = static_cast<int>(this->initialAvailableBitrate);
            auto tc = std::make_unique<webrtc::RtpTransportControllerSend>(this, nullptr,
                                                                           this->controllerFactory.get(),
                                                                           bitrateConfig);
            tc->RegisterTargetTransferRateObserver(this);
            this->probationGenerator = std::make_unique<RTC::RtpProbationGenerator>();
            // This makes sure that periodic probing is used when the application is send
            // less bitrate than needed to measure the bandwidth estimation.  (f.e. when
            // videos are muted or using screensharing with still images)
            tc->EnablePeriodicAlrProbing(true);
            
            this->processTimer = std::make_unique<TimerHandle>(this);

            // clang-format off
            this->processTimer->Start(std::min(
                // Depends on probation being done and WebRTC-Pacer-MinPacketLimitMs field trial.
                tc->packet_sender()->TimeUntilNextProcess(),
                // Fixed value (25ms), libwebrtc/api/transport/goog_cc_factory.cc.
                this->controllerFactory->GetProcessInterval().ms()
            ));
            
            this->rtpTransportControllerSend = std::move(tc);
        }
        
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            transportController->OnNetworkAvailability(true);
        }
	}

	void TransportCongestionControlClient::TransportDisconnected()
	{
		MS_TRACE();

#ifdef USE_TREND_CALCULATOR
		auto nowMs = DepLibUV::GetTimeMsInt64();
#endif

		this->bitrates.desiredBitrate          = 0u;
		this->bitrates.effectiveDesiredBitrate = 0u;

#ifdef USE_TREND_CALCULATOR
		this->desiredBitrateTrend.ForceUpdate(0u, nowMs);
#endif
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            transportController->OnNetworkAvailability(false);
        }
	}

	void TransportCongestionControlClient::InsertPacket(const webrtc::RtpPacketSendInfo& packetInfo)
	{
		MS_TRACE();
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            transportController->packet_sender()->InsertPacket(packetInfo.length);
            transportController->OnAddPacket(packetInfo);
        }
	}

	webrtc::PacedPacketInfo TransportCongestionControlClient::GetPacingInfo()
	{
		MS_TRACE();
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            return transportController->packet_sender()->GetPacingInfo();
        }
        return {};
	}

	void TransportCongestionControlClient::PacketSent(
	  const webrtc::RtpPacketSendInfo& packetInfo, int64_t nowMs)
	{
		MS_TRACE();
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            // Notify the transport feedback adapter about the sent packet.
            rtc::SentPacket const sentPacket(packetInfo.transport_sequence_number, nowMs);
            transportController->OnSentPacket(sentPacket, packetInfo.length);
        }
	}

	void TransportCongestionControlClient::ReceiveEstimatedBitrate(uint32_t bitrate)
	{
		MS_TRACE();
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            transportController->OnReceivedEstimatedBitrate(bitrate);
        }
	}

	void TransportCongestionControlClient::ReceiveRtcpReceiverReport(
	  RTC::RTCP::ReceiverReportPacket* packet, float rtt, int64_t nowMs)
	{
		MS_TRACE();
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            webrtc::ReportBlockList reportBlockList;

            for (auto it = packet->Begin(); it != packet->End(); ++it)
            {
                auto& report = *it;

                reportBlockList.emplace_back(
                  packet->GetSsrc(),
                  report->GetSsrc(),
                  report->GetFractionLost(),
                  report->GetTotalLost(),
                  report->GetLastSeq(),
                  report->GetJitter(),
                  report->GetLastSenderReport(),
                  report->GetDelaySinceLastSenderReport());
            }

            transportController->OnReceivedRtcpReceiverReport(
              reportBlockList, static_cast<int64_t>(rtt), nowMs);
        }
	}

	void TransportCongestionControlClient::ReceiveRtcpTransportFeedback(
	  const RTC::RTCP::FeedbackRtpTransportPacket* feedback)
	{
		MS_TRACE();

		// Update packet loss history.
		const size_t expectedPackets = feedback->GetPacketStatusCount();
		size_t lostPackets           = 0;

		for (const auto& result : feedback->GetPacketResults())
		{
			if (!result.received)
			{
				lostPackets += 1;
			}
		}

		if (expectedPackets > 0)
		{
			this->UpdatePacketLoss(static_cast<double>(lostPackets) / expectedPackets);
		}
        
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            transportController->OnTransportFeedback(*feedback);
        }
	}

	void TransportCongestionControlClient::UpdatePacketLoss(double packetLoss)
	{
        size_t samples{ 0 };
        double totalPacketLoss{ 0 };
        
        {
            LOCK_WRITE_PROTECTED_OBJ(this->packetLossHistory);
            // Add the score into the histogram.
            if (this->packetLossHistory->size() == PacketLossHistogramLength)
            {
                this->packetLossHistory->pop_front();
            }
            
            this->packetLossHistory->push_back(packetLoss);
            
            /*
             * Scoring mechanism is a weighted average.
             *
             * The more recent the score is, the more weight it has.
             * The oldest score has a weight of 1 and subsequent scores weight is
             * increased by one sequentially.
             *
             * Ie:
             * - scores: [1,2,3,4]
             * - this->scores = ((1) + (2+2) + (3+3+3) + (4+4+4+4)) / 10 = 2.8 => 3
             */
            
            size_t weight{ 0 };
            
            for (auto packetLossEntry : this->packetLossHistory.ConstRef())
            {
                weight++;
                samples += weight;
                totalPacketLoss += weight * packetLossEntry;
            }
        }

		// clang-tidy "thinks" that this can lead to division by zero but we are
		// smarter.
		// NOLINTNEXTLINE(clang-analyzer-core.DivideZero)
		this->packetLoss = totalPacketLoss / samples;
	}

	void TransportCongestionControlClient::SetMaxOutgoingBitrate(uint32_t maxBitrate)
	{
        if (maxBitrate != this->maxOutgoingBitrate.exchange(maxBitrate)) {
            ApplyBitrateUpdates();

            if (maxBitrate > 0u)
            {
                this->bitrates.availableBitrate = std::min<uint32_t>(maxBitrate, this->bitrates.availableBitrate);
            }

        }
	}

	void TransportCongestionControlClient::SetMinOutgoingBitrate(uint32_t minBitrate)
	{
        if (minBitrate != this->minOutgoingBitrate.exchange(minBitrate)) {
            ApplyBitrateUpdates();

            this->bitrates.minBitrate = std::max<uint32_t>(minBitrate,
                                                           RTC::TransportCongestionControlMinOutgoingBitrate);
        }
	}

	void TransportCongestionControlClient::SetDesiredBitrate(uint32_t desiredBitrate, bool force)
	{
		MS_TRACE();

#ifdef USE_TREND_CALCULATOR
		auto nowMs = DepLibUV::GetTimeMsInt64();
#endif

		// Manage it via trending and increase it a bit to avoid immediate oscillations.
#ifdef USE_TREND_CALCULATOR
		if (!force)
		{
			this->desiredBitrateTrend.Update(desiredBitrate, nowMs);
		}
		else
		{
			this->desiredBitrateTrend.ForceUpdate(desiredBitrate, nowMs);
		}
#endif

		this->bitrates.desiredBitrate = desiredBitrate;

#ifdef USE_TREND_CALCULATOR
		this->bitrates.effectiveDesiredBitrate = this->desiredBitrateTrend.GetValue();
#else
		this->bitrates.effectiveDesiredBitrate = desiredBitrate;
#endif

		this->bitrates.minBitrate = std::max<uint32_t>(
		  this->minOutgoingBitrate, RTC::TransportCongestionControlMinOutgoingBitrate);

		// NOTE: Setting 'startBitrate' to 'availableBitrate' has proven to generate
		// more stable values.
		this->bitrates.startBitrate = std::max<uint32_t>(
		  RTC::TransportCongestionControlMinOutgoingBitrate, this->bitrates.availableBitrate);

		ApplyBitrateUpdates();
	}

	void TransportCongestionControlClient::ApplyBitrateUpdates()
	{
		uint32_t currentMaxBitrate = this->bitrates.maxBitrate;
		uint32_t newMaxBitrate = 0;

#ifdef USE_TREND_CALCULATOR
		if (this->desiredBitrateTrend.GetValue() > 0u)
#else
		if (this->bitrates.desiredBitrate > 0u)
#endif
		{
			newMaxBitrate = std::max<uint32_t>(
			  this->initialAvailableBitrate,
#ifdef USE_TREND_CALCULATOR
			  this->desiredBitrateTrend.GetValue() * MaxBitrateIncrementFactor);
#else
			  this->bitrates.desiredBitrate * MaxBitrateIncrementFactor);
#endif

			// If max bitrate requested didn't change by more than a small % keep the
			// previous settings to avoid constant small fluctuations requiring extra
			// probing and making the estimation less stable (requires constant
			// redistribution of bitrate accross consumers).
			auto maxBitrateMargin = newMaxBitrate * MaxBitrateMarginFactor;
			if (currentMaxBitrate > newMaxBitrate - maxBitrateMargin && currentMaxBitrate < newMaxBitrate + maxBitrateMargin)
			{
				newMaxBitrate = currentMaxBitrate;
			}
		}
		else
		{
			newMaxBitrate = this->initialAvailableBitrate;
		}

		if (this->maxOutgoingBitrate > 0u)
		{
			newMaxBitrate = std::min<uint32_t>(this->maxOutgoingBitrate, newMaxBitrate);
		}

		if (newMaxBitrate != currentMaxBitrate)
		{
			this->bitrates.maxPaddingBitrate = newMaxBitrate * MaxPaddingBitrateFactor;
			this->bitrates.maxBitrate        = newMaxBitrate;
		}

		this->bitrates.minBitrate = std::max<uint32_t>(
		  this->minOutgoingBitrate, RTC::TransportCongestionControlMinOutgoingBitrate);

		MS_DEBUG_DEV(
		  "[desiredBitrate:%" PRIu32 ", desiredBitrateTrend:%" PRIu32 ", startBitrate:%" PRIu32
		  ", minBitrate:%" PRIu32 ", maxBitrate:%" PRIu32 ", maxPaddingBitrate:%" PRIu32 "]",
		  this->bitrates.desiredBitrate.load(),
		  this->desiredBitrateTrend.GetValue(),
		  this->bitrates.startBitrate.load(),
		  this->bitrates.minBitrate.load(),
		  this->bitrates.maxBitrate.load(),
		  this->bitrates.maxPaddingBitrate.load());
        
        LOCK_WRITE_PROTECTED_OBJ(this->rtpTransportControllerSend);
        if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
            transportController->SetAllocatedSendBitrateLimits(
              this->bitrates.minBitrate, this->bitrates.maxPaddingBitrate, this->bitrates.maxBitrate);

            webrtc::TargetRateConstraints constraints;

            constraints.at_time       = webrtc::Timestamp::ms(DepLibUV::GetTimeMs());
            constraints.min_data_rate = webrtc::DataRate::bps(this->bitrates.minBitrate.load());
            constraints.max_data_rate = webrtc::DataRate::bps(this->bitrates.maxBitrate.load());
            constraints.starting_rate = webrtc::DataRate::bps(this->bitrates.startBitrate.load());

            transportController->SetClientBitratePreferences(constraints);
        }
	}

	uint32_t TransportCongestionControlClient::GetAvailableBitrate() const
	{
		MS_TRACE();

		return this->bitrates.availableBitrate;
	}

	double TransportCongestionControlClient::GetPacketLoss() const
	{
		MS_TRACE();

		return this->packetLoss;
	}

	void TransportCongestionControlClient::RescheduleNextAvailableBitrateEvent()
	{
		MS_TRACE();

		this->lastAvailableBitrateEventAtMs = DepLibUV::GetTimeMs();
	}

	void TransportCongestionControlClient::MayEmitAvailableBitrateEvent(uint32_t previousAvailableBitrate)
	{
		MS_TRACE();

		const uint64_t nowMs = DepLibUV::GetTimeMsInt64();
		bool notify{ false };

		// Ignore if first event.
		// NOTE: Otherwise it will make the Transport crash since this event also happens
		// during the constructor of this class.
		if (this->lastAvailableBitrateEventAtMs == 0u)
		{
			this->lastAvailableBitrateEventAtMs = nowMs;

			return;
		}

		// Emit if this is the first valid event.
		if (!this->availableBitrateEventCalled.exchange(true))
		{
			notify = true;
		}
		// Emit event if AvailableBitrateEventInterval elapsed.
		else if (nowMs - this->lastAvailableBitrateEventAtMs >= AvailableBitrateEventInterval)
		{
			notify = true;
		}
		// Also emit the event fast if we detect a high BWE value decrease.
		else if (this->bitrates.availableBitrate < previousAvailableBitrate * 0.75)
		{
			MS_WARN_TAG(
			  bwe,
			  "high BWE value decrease detected, notifying the listener [now:%" PRIu32 ", before:%" PRIu32
			  "]",
			  this->bitrates.availableBitrate.load(),
			  previousAvailableBitrate);

			notify = true;
		}
		// Also emit the event fast if we detect a high BWE value increase.
		else if (this->bitrates.availableBitrate > previousAvailableBitrate * 1.50)
		{
			MS_DEBUG_TAG(
			  bwe,
			  "high BWE value increase detected, notifying the listener [now:%" PRIu32 ", before:%" PRIu32
			  "]",
			  this->bitrates.availableBitrate.load(),
			  previousAvailableBitrate);

			notify = true;
		}

		if (notify)
		{
			MS_DEBUG_DEV(
			  "notifying the listener with new available bitrate:%" PRIu32,
			  this->bitrates.availableBitrate.load());

			this->lastAvailableBitrateEventAtMs = nowMs;

			this->listener->OnTransportCongestionControlClientBitrates(this, this->bitrates);
		}
	}

	void TransportCongestionControlClient::OnTargetTransferRate(webrtc::TargetTransferRate targetTransferRate)
	{
		MS_TRACE();

		// NOTE: The same value as 'this->initialAvailableBitrate' is received periodically
		// regardless of the real available bitrate. Skip such value except for the first time
		// this event is called.
		// clang-format off
		if (
			this->availableBitrateEventCalled &&
			targetTransferRate.target_rate.bps() == this->initialAvailableBitrate
		)
		// clang-format on
		{
			return;
		}

		uint32_t previousAvailableBitrate = this->bitrates.availableBitrate;

		// Update availableBitrate.
		// NOTE: Just in case.
		if (targetTransferRate.target_rate.bps() > std::numeric_limits<uint32_t>::max())
		{
			this->bitrates.availableBitrate = std::numeric_limits<uint32_t>::max();
		}
		else
		{
			this->bitrates.availableBitrate = static_cast<uint32_t>(targetTransferRate.target_rate.bps());
		}

		MS_DEBUG_DEV("new available bitrate:%" PRIu32, this->bitrates.availableBitrate.load());

		MayEmitAvailableBitrateEvent(previousAvailableBitrate);
	}

	// Called from PacedSender in order to send probation packets.
	void TransportCongestionControlClient::SendPacket(
	  RTC::RtpPacket* packet, const webrtc::PacedPacketInfo& pacingInfo)
	{
		MS_TRACE();

		// Send the packet.
		this->listener->OnTransportCongestionControlClientSendRtpPacket(this, packet, pacingInfo);
	}

	RTC::RtpPacket* TransportCongestionControlClient::GeneratePadding(size_t size)
	{
		MS_TRACE();
        LOCK_READ_PROTECTED_OBJ(rtpTransportControllerSend);
		MS_ASSERT(this->probationGenerator, "probation generator not initialized")

		return this->probationGenerator->GetNextPacket(size);
	}

	void TransportCongestionControlClient::OnTimer(TimerHandle* timer)
	{
		MS_TRACE();
        bool ok = false;
        {
            LOCK_WRITE_PROTECTED_OBJ(rtpTransportControllerSend);
            if (const auto& transportController = this->rtpTransportControllerSend.ConstRef()) {
                if (timer == this->processTimer.get())
                {
                    // Time to call RtpTransportControllerSend::Process().
                    transportController->Process();
                    
                    // Time to call PacedSender::Process().
                    transportController->packet_sender()->Process();
                    
                    /* clang-format off */
                    this->processTimer->Start(std::min<uint64_t>(
                        // Depends on probation being done and WebRTC-Pacer-MinPacketLimitMs field trial.
                        transportController->packet_sender()->TimeUntilNextProcess(),
                        // Fixed value (25ms), libwebrtc/api/transport/goog_cc_factory.cc.
                        this->controllerFactory->GetProcessInterval().ms()
                     ));
                    /* clang-format on */
                    ok = true;
                }
            }
        }
        if (ok) {
            MayEmitAvailableBitrateEvent(this->bitrates.availableBitrate);
        }
	}
} // namespace RTC
