#ifndef MS_RTC_RTP_STREAM_SEND_HPP
#define MS_RTC_RTP_STREAM_SEND_HPP

#include "RTC/RateCalculator.hpp"
#include "RTC/RtpRetransmissionBuffer.hpp"
#include "RTC/RtpStream.hpp"
#include "ProtectedObj.hpp"

namespace RTC
{
	class RtpStreamSend : public RTC::RtpStream
	{
	public:
		// Maximum retransmission buffer size for video (ms).
		const static uint32_t MaxRetransmissionDelayForVideoMs;
		// Maximum retransmission buffer size for audio (ms).
		const static uint32_t MaxRetransmissionDelayForAudioMs;

	public:
		class Listener : public RTC::RtpStream::Listener
		{
		public:
			virtual void OnRtpStreamRetransmitRtpPacket(
			  RTC::RtpStreamSend* rtpStream, RTC::RtpPacket* packet) = 0;
		};

	public:
		RtpStreamSend(
		  RTC::RtpStreamSend::Listener* listener, const RTC::RtpStream::Params& params, const std::string& mid);
		~RtpStreamSend() override;

		flatbuffers::Offset<FBS::RtpStream::Stats> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder) override;
		void SetRtx(uint8_t payloadType, uint32_t ssrc) override;
		bool ReceivePacket(RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket);
		void ReceiveNack(const RTC::RTCP::FeedbackRtpNackPacket* nackPacket);
		void ReceiveKeyFrameRequest(const RTC::RTCP::FeedbackPs::MessageType messageType);
		void ReceiveRtcpReceiverReport(const RTC::RTCP::ReceiverReport* report);
		void ReceiveRtcpXrReceiverReferenceTime(const RTC::RTCP::ReceiverReferenceTime* report);
		RTC::RTCP::SenderReport* GetRtcpSenderReport(uint64_t nowMs);
		RTC::RTCP::DelaySinceLastRr::SsrcInfo* GetRtcpXrDelaySinceLastRrSsrcInfo(uint64_t nowMs);
		RTC::RTCP::SdesChunk* GetRtcpSdesChunk();
		void Pause() override;
		void Resume() override;
        uint32_t GetBitrate(uint64_t nowMs) override;
		uint32_t GetBitrate(uint64_t nowMs, uint8_t spatialLayer, uint8_t temporalLayer) override;
		uint32_t GetSpatialLayerBitrate(uint64_t nowMs, uint8_t spatialLayer) override;
		uint32_t GetLayerBitrate(uint64_t nowMs, uint8_t spatialLayer, uint8_t temporalLayer) override;

	private:
		void StorePacket(RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket);
		void FillRetransmissionContainer(uint16_t seq, uint16_t bitmask);
		void UpdateScore(const RTC::RTCP::ReceiverReport* report);

		/* Pure virtual methods inherited from RTC::RtpStream. */
	public:
		void UserOnSequenceNumberReset() override;

	private:
        const std::string mid;
		// Packets lost at last interval for score calculation.
        uint32_t lostPriorScore{ 0u };
		// Packets sent at last interval for score calculation.
		uint32_t sentPriorScore{ 0u };
        uint16_t rtxSeq{ 0u };
        RTC::RtpDataCounter transmissionCounter;
		ProtectedUniquePtr<RTC::RtpRetransmissionBuffer> retransmissionBuffer{ nullptr };
		// The middle 32 bits out of 64 in the NTP timestamp received in the most
		// recent receiver reference timestamp.
		std::atomic<uint32_t> lastRrTimestamp{ 0u };
		// Wallclock time representing the most recent receiver reference timestamp
		// arrival.
        std::atomic<uint64_t> lastRrReceivedMs{ 0u };
	};
} // namespace RTC

#endif
