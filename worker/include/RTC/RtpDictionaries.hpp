#ifndef MS_RTC_RTP_DICTIONARIES_HPP
#define MS_RTC_RTP_DICTIONARIES_HPP

#include "common.hpp"
#include "FBS/rtpParameters.h"
#include "RTC/Parameters.hpp"
#include <absl/container/flat_hash_map.h>
#include <string>
#include <vector>

namespace RTC
{
	class Media
	{
	public:
		enum class Kind : uint8_t
		{
			AUDIO,
			VIDEO
		};
	};

	class RtpCodecMimeType
	{
	public:
		enum class Type : uint8_t
		{
			AUDIO,
			VIDEO
		};

	public:
		enum class Subtype : uint16_t
		{
			// Audio codecs:
			OPUS = 100,
			// Multi-channel Opus.
			MULTIOPUS,
			PCMA,
			PCMU,
			ISAC,
			G722,
			ILBC,
			SILK,
			// Video codecs:
			VP8 = 200,
			VP9,
			H264,
			H264_SVC,
			X_H264UC,
			H265,
			// Complementary codecs:
			CN = 300,
			TELEPHONE_EVENT,
			// Feature codecs:
			RTX = 400,
			ULPFEC,
			X_ULPFECUC,
			FLEXFEC,
			RED
		};

	public:
		static absl::flat_hash_map<std::string, Type> string2Type;
		static absl::flat_hash_map<Type, std::string> type2String;
		static absl::flat_hash_map<std::string, Subtype> string2Subtype;
		static absl::flat_hash_map<Subtype, std::string> subtype2String;

	public:
		RtpCodecMimeType() = delete;
        
        RtpCodecMimeType(const RtpCodecMimeType& other) = default;
        
        RtpCodecMimeType(Type type, Subtype subtype);
        
        RtpCodecMimeType(const std::string& mimeType);

        bool operator==(const RtpCodecMimeType& other) const
        {
            return GetType() == other.GetType() && GetSubtype() == other.GetSubtype();
        }

        bool operator!=(const RtpCodecMimeType& other) const
        {
            return GetType() != other.GetType() || GetSubtype() != other.GetSubtype();
        }

        Type GetType() const { return this->type; }
                
        Subtype GetSubtype() const { return this->subtype; }
        
        void SetType(Type type) { this->type = type; }
        
        void SetSubType(Subtype subtype) { this->subtype = subtype; }
        
        void SetMimeType(const std::string& mimeType);

        std::string ToString() const;

        bool IsMediaCodec() const
        {
            return IsAudioCodec() || IsVideoCodec();
        }
        
        bool IsAudioCodec() const
        {
            return GetSubtype() >= Subtype(100) && GetSubtype() < Subtype(200);
        }
        
        bool IsVideoCodec() const
        {
            return GetSubtype() >= Subtype(200) && GetSubtype() < Subtype(300);
        }

        bool IsComplementaryCodec() const
        {
            return GetSubtype() >= Subtype(300) && GetSubtype() < Subtype(400);
        }

        bool IsFeatureCodec() const
        {
            return GetSubtype() >= Subtype(400);
        }

	private:
        Type type;
        Subtype subtype;
	};

	class RtpHeaderExtensionUri
	{
	public:
		enum class Type : uint8_t
		{
			MID                    = 1,
			RTP_STREAM_ID          = 2,
			REPAIRED_RTP_STREAM_ID = 3,
			ABS_SEND_TIME          = 4,
			TRANSPORT_WIDE_CC_01   = 5,
			FRAME_MARKING_07       = 6, // NOTE: Remove once RFC.
			FRAME_MARKING          = 7,
			SSRC_AUDIO_LEVEL       = 10,
			VIDEO_ORIENTATION      = 11,
			TOFFSET                = 12,
			ABS_CAPTURE_TIME       = 13,
		};

	public:
		static RtpHeaderExtensionUri::Type TypeFromFbs(FBS::RtpParameters::RtpHeaderExtensionUri uri);
		static FBS::RtpParameters::RtpHeaderExtensionUri TypeToFbs(RtpHeaderExtensionUri::Type uri);
	};

	class RtcpFeedback
	{
	public:
		RtcpFeedback() = default;
		explicit RtcpFeedback(const FBS::RtpParameters::RtcpFeedback* data);

		flatbuffers::Offset<FBS::RtpParameters::RtcpFeedback> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

	public:
		std::string type;
		std::string parameter;
	};

	class RtpCodecParameters
	{
	public:
		RtpCodecParameters() = delete;
		explicit RtpCodecParameters(const FBS::RtpParameters::RtpCodecParameters* data);

		flatbuffers::Offset<FBS::RtpParameters::RtpCodecParameters> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

	private:
		void CheckCodec();

	public:
		RtpCodecMimeType mimeType;
		uint8_t payloadType{ 0u };
		uint32_t clockRate{ 0u };
		uint8_t channels{ 1u };
		RTC::Parameters parameters;
		std::vector<RtcpFeedback> rtcpFeedback;
	};

	class RtpRtxParameters
	{
	public:
		RtpRtxParameters() = default;
		explicit RtpRtxParameters(const FBS::RtpParameters::Rtx* data);

		flatbuffers::Offset<FBS::RtpParameters::Rtx> FillBuffer(flatbuffers::FlatBufferBuilder& builder) const;

	public:
		uint32_t ssrc{ 0u };
	};

	class RtpEncodingParameters
	{
	public:
		RtpEncodingParameters() = default;
		explicit RtpEncodingParameters(const FBS::RtpParameters::RtpEncodingParameters* data);

		flatbuffers::Offset<FBS::RtpParameters::RtpEncodingParameters> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

	public:
		uint32_t ssrc{ 0u };
		std::string rid;
		uint8_t codecPayloadType{ 0u };
		bool hasCodecPayloadType{ false };
		RtpRtxParameters rtx;
		bool hasRtx{ false };
		uint32_t maxBitrate{ 0u };
		double maxFramerate{ 0 };
		bool dtx{ false };
		std::string scalabilityMode{ "S1T1" };
		uint8_t spatialLayers{ 1u };
		uint8_t temporalLayers{ 1u };
		bool ksvc{ false };
	};

	class RtpHeaderExtensionParameters
	{
	public:
		RtpHeaderExtensionParameters() = default;
		explicit RtpHeaderExtensionParameters(
		  const FBS::RtpParameters::RtpHeaderExtensionParameters* const data);

		flatbuffers::Offset<FBS::RtpParameters::RtpHeaderExtensionParameters> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

	public:
		RtpHeaderExtensionUri::Type type;
		uint8_t id{ 0u };
		bool encrypt{ false };
		RTC::Parameters parameters;
	};

	class RtcpParameters
	{
	public:
		RtcpParameters() = default;
		explicit RtcpParameters(const FBS::RtpParameters::RtcpParameters* data);

		flatbuffers::Offset<FBS::RtpParameters::RtcpParameters> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

	public:
		std::string cname;
		bool reducedSize{ true };
	};

	class RtpParameters
	{
	public:
		enum class Type : uint8_t
		{
			SIMPLE,
			SIMULCAST,
			SVC,
			PIPE
		};

	public:
		static std::optional<Type> GetType(const RtpParameters& rtpParameters);
		static std::string& GetTypeString(Type type);
		static FBS::RtpParameters::Type TypeToFbs(Type type);

	private:
		static absl::flat_hash_map<Type, std::string> type2String;

	public:
		RtpParameters() = default;
		explicit RtpParameters(const FBS::RtpParameters::RtpParameters* data);

		flatbuffers::Offset<FBS::RtpParameters::RtpParameters> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		const RTC::RtpCodecParameters* GetCodecForEncoding(const RtpEncodingParameters& encoding) const;
		const RTC::RtpCodecParameters* GetRtxCodecForEncoding(const RtpEncodingParameters& encoding) const;

	private:
		void ValidateCodecs();
		void ValidateEncodings();
		void SetType();

	public:
		std::string mid;
		std::vector<RtpCodecParameters> codecs;
		std::vector<RtpEncodingParameters> encodings;
		std::vector<RtpHeaderExtensionParameters> headerExtensions;
		RtcpParameters rtcp;
	};
} // namespace RTC

#endif
