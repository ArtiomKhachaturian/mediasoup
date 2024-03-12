#ifndef MS_RTC_SIMPLE_CONSUMER_HPP
#define MS_RTC_SIMPLE_CONSUMER_HPP

#include "FBS/transport.h"
#include "RTC/Consumer.hpp"
#include "RTC/SeqManager.hpp"
#include "RTC/Shared.hpp"

namespace RTC
{
	class SimpleConsumer : public RTC::Consumer, public RTC::RtpStreamSend::Listener
	{
	public:
		SimpleConsumer(
		  RTC::Shared* shared,
		  const std::string& id,
		  const std::string& producerId,
		  RTC::Consumer::Listener* listener,
		  const FBS::Transport::ConsumeRequest* data);
		~SimpleConsumer() override;

	public:
		flatbuffers::Offset<FBS::Consumer::DumpResponse> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		flatbuffers::Offset<FBS::Consumer::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder) override;
		flatbuffers::Offset<FBS::Consumer::ConsumerScore> FillBufferScore(
		  flatbuffers::FlatBufferBuilder& builder) const override;
        bool IsActive() const override;
		void ProducerRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc) override;
		void ProducerNewRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc) override;
		void ProducerRtpStreamScore(
		  RTC::RtpStreamRecv* rtpStream, uint8_t score, uint8_t previousScore) override;
		void ProducerRtcpSenderReport(RTC::RtpStreamRecv* rtpStream, bool first) override;
		uint8_t GetBitratePriority() const override;
		uint32_t IncreaseLayer(uint32_t bitrate, bool considerLoss) override;
		void ApplyLayers() override;
		uint32_t GetDesiredBitrate() const override;
		void SendRtpPacket(RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket) override;
		bool GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t nowMs) override;
		void NeedWorstRemoteFractionLost(uint32_t mappedSsrc, uint8_t& worstRemoteFractionLost) override;
		void ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket) override;
		void ReceiveKeyFrameRequest(RTC::RTCP::FeedbackPs::MessageType messageType, uint32_t ssrc) override;
		void ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report) override;
		void ReceiveRtcpXrReceiverReferenceTime(RTC::RTCP::ReceiverReferenceTime* report) override;
		uint32_t GetTransmissionRate(uint64_t nowMs) override;
		float GetRtt() const override;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

	private:
		void UserOnTransportConnected() override;
		void UserOnTransportDisconnected() override;
		void UserOnPaused() override;
		void UserOnResumed() override;
		void RequestKeyFrame();
		void EmitScore() const;
        static const RTC::RtpCodecParameters* Get1stCodec(const RTC::RtpParameters& rtpParameters);
        static bool IsKeyFrameSupported(const RTC::RtpParameters& rtpParameters);
        static std::unique_ptr<RTC::Codecs::EncodingContext> CreateEncodingContext(const RTC::RtpParameters& rtpParameters);
        static std::unique_ptr<RTC::RtpStreamSend> CreateRtpStream(const RTC::RtpCodecParameters* codec,
                                                                   const RTC::RtpEncodingParameters& encoding,
                                                                   const std::string& cname,
                                                                   const std::string& mid,
                                                                   RTC::RtpStreamSend::Listener* listener);
        static std::unique_ptr<RTC::RtpStreamSend> CreateRtpStream(const RTC::RtpParameters& rtpParameters,
                                                                   RTC::RtpStreamSend::Listener* listener);

		/* Pure virtual methods inherited from RtpStreamSend::Listener. */
	public:
		void OnRtpStreamScore(RTC::RtpStream* rtpStream, uint8_t score, uint8_t previousScore) override;
		void OnRtpStreamRetransmitRtpPacket(RTC::RtpStreamSend* rtpStream, RTC::RtpPacket* packet) override;

	private:
        const std::unique_ptr<RTC::Codecs::EncodingContext> encodingContext;
        const bool keyFrameSupported;
		// Allocated by this.
        const std::unique_ptr<RTC::RtpStreamSend> rtpStream;
		// Others.
		ProtectedObj<RTC::RtpStreamRecv*, std::mutex> producerRtpStream{ nullptr };
		std::atomic_bool syncRequired{ false };
        ProtectedObj<RTC::SeqManager<uint16_t>, std::mutex> rtpSeqManager;
        std::atomic_bool managingBitrate{ false };
	};
} // namespace RTC

#endif
