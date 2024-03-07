#ifndef MS_RTC_CONSUMER_HPP
#define MS_RTC_CONSUMER_HPP

#include "common.hpp"
#include "Channel/ChannelRequest.hpp"
#include "Channel/ChannelSocket.hpp"
#include "FBS/consumer.h"
#include "RTC/RTCP/CompoundPacket.hpp"
#include "RTC/RTCP/FeedbackPs.hpp"
#include "RTC/RTCP/FeedbackPsFir.hpp"
#include "RTC/RTCP/FeedbackPsPli.hpp"
#include "RTC/RTCP/FeedbackRtpNack.hpp"
#include "RTC/RTCP/ReceiverReport.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpHeaderExtensionIds.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtpStream.hpp"
#include "RTC/RtpStreamRecv.hpp"
#include "RTC/RtpStreamSend.hpp"
#include "RTC/Shared.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_set.h>
#include <atomic>
#include <string>
#include <vector>

namespace RTC
{
	class Consumer : public Channel::ChannelSocket::RequestHandler
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnConsumerSendRtpPacket(RTC::Consumer* consumer, RTC::RtpPacket* packet) = 0;
			virtual void OnConsumerRetransmitRtpPacket(RTC::Consumer* consumer, RTC::RtpPacket* packet) = 0;
			virtual void OnConsumerKeyFrameRequested(RTC::Consumer* consumer, uint32_t mappedSsrc) = 0;
			virtual void OnConsumerNeedBitrateChange(RTC::Consumer* consumer)                      = 0;
			virtual void OnConsumerNeedZeroBitrate(RTC::Consumer* consumer)                        = 0;
			virtual void OnConsumerProducerClosed(RTC::Consumer* consumer)                         = 0;
		};

	public:
		struct Layers
		{
			int16_t spatial{ -1 };
			int16_t temporal{ -1 };
		};

	private:
		struct TraceEventTypes
		{
			bool rtp{ false };
			bool keyframe{ false };
			bool nack{ false };
			bool pli{ false };
			bool fir{ false };
		};

	public:
		Consumer(
		  RTC::Shared* shared,
		  const std::string& id,
		  const std::string& producerId,
		  RTC::Consumer::Listener* listener,
		  const FBS::Transport::ConsumeRequest* data,
		  RTC::RtpParameters::Type type);
		~Consumer() override;

	public:
		flatbuffers::Offset<FBS::Consumer::BaseConsumerDump> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		virtual flatbuffers::Offset<FBS::Consumer::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder) = 0;
		virtual flatbuffers::Offset<FBS::Consumer::ConsumerScore> FillBufferScore(
		  flatbuffers::FlatBufferBuilder& /*builder*/) const
		{
			return 0;
		};
		RTC::Media::Kind GetKind() const
		{
			return this->kind;
		}
		const RTC::RtpParameters& GetRtpParameters() const
		{
			return this->rtpParameters;
		}
		const struct RTC::RtpHeaderExtensionIds& GetRtpHeaderExtensionIds() const
		{
			return this->rtpHeaderExtensionIds;
		}
		RTC::RtpParameters::Type GetType() const
		{
			return this->type;
		}
		virtual Layers GetPreferredLayers() const
		{
			// By default return 1:1.
			Consumer::Layers layers;

			return layers;
		}
		const std::vector<uint32_t>& GetMediaSsrcs() const
		{
			return this->mediaSsrcs;
		}
		const std::vector<uint32_t>& GetRtxSsrcs() const
		{
			return this->rtxSsrcs;
		}
		virtual bool IsActive() const
		{
			// The parent Consumer just checks whether Consumer and Producer are
			// not paused and the transport connected.
			// clang-format off
			return (
				this->transportConnected &&
				!this->paused &&
				!this->producerPaused &&
				!this->producerClosed
			);
			// clang-format on
		}
		void TransportConnected();
		void TransportDisconnected();
		bool IsPaused() const
		{
			return this->paused;
		}
		bool IsProducerPaused() const
		{
			return this->producerPaused;
		}
		void ProducerPaused();
		void ProducerResumed();
        std::string GetLanguageId() const;
        void SetLanguageId(std::string languageId);
        std::string GetVoiceId() const;
        void SetVoiceId(std::string voiceId);
		virtual void ProducerRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc)    = 0;
		virtual void ProducerNewRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc) = 0;
		void ProducerRtpStreamScores(const std::vector<uint8_t>* scores);
		virtual void ProducerRtpStreamScore(
		  RTC::RtpStreamRecv* rtpStream, uint8_t score, uint8_t previousScore)           = 0;
		virtual void ProducerRtcpSenderReport(RTC::RtpStreamRecv* rtpStream, bool first) = 0;
		void ProducerClosed();
		void SetExternallyManagedBitrate()
		{
			this->externallyManagedBitrate = true;
		}
		virtual uint8_t GetBitratePriority() const                          = 0;
		virtual uint32_t IncreaseLayer(uint32_t bitrate, bool considerLoss) = 0;
		virtual void ApplyLayers()                                          = 0;
		virtual uint32_t GetDesiredBitrate() const                          = 0;
		virtual void SendRtpPacket(RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket) = 0;
		virtual bool GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t nowMs) = 0;
		virtual void NeedWorstRemoteFractionLost(uint32_t mappedSsrc, uint8_t& worstRemoteFractionLost) = 0;
		virtual void ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket) = 0;
		virtual void ReceiveKeyFrameRequest(
		  RTC::RTCP::FeedbackPs::MessageType messageType, uint32_t ssrc)                          = 0;
		virtual void ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report)                 = 0;
		virtual void ReceiveRtcpXrReceiverReferenceTime(RTC::RTCP::ReceiverReferenceTime* report) = 0;
		virtual uint32_t GetTransmissionRate(uint64_t nowMs)                                      = 0;
		virtual float GetRtt() const                                                              = 0;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

	protected:
		void EmitTraceEventRtpAndKeyFrameTypes(RTC::RtpPacket* packet, bool isRtx = false) const;
		void EmitTraceEventKeyFrameType(RTC::RtpPacket* packet, bool isRtx = false) const;
		void EmitTraceEventPliType(uint32_t ssrc) const;
		void EmitTraceEventFirType(uint32_t ssrc) const;
		void EmitTraceEventNackType() const;
		void EmitTraceEvent(flatbuffers::Offset<FBS::Consumer::TraceNotification>& notification) const;

	private:
        static std::vector<RTC::RtpEncodingParameters> GetEncodingParameters(const FBS::Transport::ConsumeRequest* data) noexcept(false);
        static std::bitset<128u> GetSupportedCodecPayloadTypes(const std::vector<RtpCodecParameters>& codecs);
        static std::vector<uint32_t> GetSsrcs(const std::vector<RtpEncodingParameters>& encodings, bool rtx);
		virtual void UserOnTransportConnected()    = 0;
		virtual void UserOnTransportDisconnected() = 0;
		virtual void UserOnPaused()                = 0;
		virtual void UserOnResumed()               = 0;

	public:
		// Passed by argument.
		const std::string id;
		const std::string producerId;

	protected:
		// Passed by argument.
		RTC::Shared* const shared;
		RTC::Consumer::Listener* const listener;
		const RTC::Media::Kind kind;
        const RTC::RtpParameters::Type type;
        const uint16_t maxRtcpInterval;
        // Others.
		const RTC::RtpParameters rtpParameters;
		const std::vector<RTC::RtpEncodingParameters> consumableRtpEncodings;
		struct RTC::RtpHeaderExtensionIds rtpHeaderExtensionIds;
        // Whether a payload type is supported or not is represented in the
        // corresponding position of the bitset.
        const std::bitset<128u> supportedCodecPayloadTypes;
		const std::vector<uint8_t>* producerRtpStreamScores{ nullptr };
		// Others.
		std::atomic<uint64_t> lastRtcpSentTime{ 0u };
        std::atomic_bool externallyManagedBitrate{ false };
        std::atomic<uint8_t> priority{ 1u };
		struct TraceEventTypes traceEventTypes;

	private:
		// Others.
		const std::vector<uint32_t> mediaSsrcs;
        const std::vector<uint32_t> rtxSsrcs;
		std::atomic_bool transportConnected{ false };
        std::atomic_bool paused{ false };
        std::atomic_bool producerPaused{ false };
        std::atomic_bool producerClosed{ false };
        // both language & voice should be defined explictly for translation service,
        // otherwise translation is not available
        ProtectedObj<std::string> languageId;
        ProtectedObj<std::string> voiceId;
	};
} // namespace RTC

#endif
