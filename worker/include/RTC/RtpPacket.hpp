#ifndef MS_RTC_RTP_PACKET_HPP
#define MS_RTC_RTP_PACKET_HPP

#include "common.hpp"
#include "Utils.hpp"
#include "FBS/rtpPacket.h"
#include "RTC/Codecs/PayloadDescriptorHandler.hpp"
#include "RTC/RtpPacketHeader.hpp"
#ifdef MS_RTC_LOGGER_RTP
#include "RTC/RtcLogger.hpp"
#endif
#include <flatbuffers/flatbuffers.h>
#include <absl/container/flat_hash_map.h>
#include <array>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace RTC
{
    class Buffer;
    class BufferAllocator;
    class Consumer;
	// Max MTU size.
	constexpr size_t MtuSize{ 1500u };
	// MID header extension max length (just used when setting/updating MID
	// extension).
	constexpr uint8_t MidMaxLength{ 8u };

	class RtpPacket
	{
	protected:
		/* Struct for RTP header extension. */
		struct HeaderExtension
		{
			uint16_t id;
			uint16_t length; // Size of value in multiples of 4 bytes.
			uint8_t value[1];
		};

	private:
		/* Struct for One-Byte extension. */
		struct OneByteExtension
		{
#if defined(MS_LITTLE_ENDIAN)
			uint8_t len : 4;
			uint8_t id : 4;
#elif defined(MS_BIG_ENDIAN)
			uint8_t id : 4;
			uint8_t len : 4;
#endif
			uint8_t value[1];
		};

	private:
		/* Struct for Two-Bytes extension. */
		struct TwoBytesExtension
		{
			uint8_t id;
			uint8_t len;
			uint8_t value[1];
		};

	public:
		/* Struct for replacing and setting header extensions. */
		struct GenericExtension
		{
			GenericExtension(uint8_t id, uint8_t len, uint8_t* value) : id(id), len(len), value(value){};

			uint8_t id;
			uint8_t len;
			uint8_t* value;
		};

	public:
		/* Struct with frame-marking information. */
		struct FrameMarking
		{
#if defined(MS_LITTLE_ENDIAN)
			uint8_t tid : 3;
			uint8_t base : 1;
			uint8_t discardable : 1;
			uint8_t independent : 1;
			uint8_t end : 1;
			uint8_t start : 1;
#elif defined(MS_BIG_ENDIAN)
			uint8_t start : 1;
			uint8_t end : 1;
			uint8_t independent : 1;
			uint8_t discardable : 1;
			uint8_t base : 1;
			uint8_t tid : 3;
#endif
			uint8_t lid;
			uint8_t tl0picidx;
		};

	public:
        // NOTE: RtcpPacket::IsRtcp() must always be called before this method.
		static bool IsRtp(const uint8_t* data, size_t len)
		{
            return RtpPacketHeader::IsRtp(data, len);
		}

		static RtpPacket* Parse(const uint8_t* data, size_t len,
                                const std::shared_ptr<BufferAllocator>& allocator = nullptr);
	public:
        RtpPacket(
          RtpPacketHeader* header,
          HeaderExtension* headerExtension,
          const uint8_t* payload,
          size_t payloadLength,
          uint8_t payloadPadding,
          size_t size,
          const std::shared_ptr<BufferAllocator>& allocator = nullptr);
        ~RtpPacket();

        void AddAcceptedConsumer(Consumer* consumer);
        void SetAcceptedConsumers(std::unordered_set<Consumer*> consumers);
        bool ConsumerIsAccepted(Consumer* consumer) const;
        
		void Dump() const;
		flatbuffers::Offset<FBS::RtpPacket::Dump> FillBuffer(flatbuffers::FlatBufferBuilder& builder) const;

		const uint8_t* GetData() const
		{
			return (const uint8_t*)this->header;
		}

		size_t GetSize() const
		{
			return this->size;
		}

		uint8_t GetPayloadType() const
		{
			return this->header->GetPayloadType();
		}

		void SetPayloadType(uint8_t payloadType)
		{
			this->header->SetPayloadType(payloadType);
		}

		bool HasMarker() const
		{
			return this->header->HasMarker();
		}

		void SetMarker(bool marker)
		{
			this->header->SetMarker(marker);
		}

		void SetPayloadPaddingFlag(bool flag)
		{
            this->header->SetPayloadPadding(flag);
		}

		uint16_t GetSequenceNumber() const
		{
            return this->header->GetSequenceNumber();
		}

		void SetSequenceNumber(uint16_t seq)
		{
            this->header->SetSequenceNumber(seq);
		}

		uint32_t GetTimestamp() const
		{
            return this->header->GetTimestamp();
		}

		void SetTimestamp(uint32_t timestamp)
		{
            this->header->SetTimestamp(timestamp);
		}

		uint32_t GetSsrc() const
		{
            return this->header->GetSsrc();
		}

		void SetSsrc(uint32_t ssrc)
		{
            this->header->SetSsrc(ssrc);
		}

		bool HasHeaderExtension() const
		{
			return (this->headerExtension != nullptr);
		}

		// After calling this method, all the extension ids are reset to 0.
		void SetExtensions(uint8_t type, const std::vector<GenericExtension>& extensions);

		uint16_t GetHeaderExtensionId() const
		{
			if (!this->headerExtension)
			{
				return 0u;
			}

			return uint16_t{ ntohs(this->headerExtension->id) };
		}

		size_t GetHeaderExtensionLength() const
		{
            return GetHeaderExtensionLength(this->headerExtension);
		}

		uint8_t* GetHeaderExtensionValue() const
		{
			if (!this->headerExtension)
			{
				return nullptr;
			}

			return this->headerExtension->value;
		}

		bool HasOneByteExtensions() const
		{
			return GetHeaderExtensionId() == 0xBEDE;
		}

		bool HasTwoBytesExtensions() const
		{
			return (GetHeaderExtensionId() & 0b1111111111110000) == 0b0001000000000000;
		}

		void SetMidExtensionId(uint8_t id)
		{
			this->midExtensionId = id;
		}

		void SetRidExtensionId(uint8_t id)
		{
			this->ridExtensionId = id;
		}

		void SetRepairedRidExtensionId(uint8_t id)
		{
			this->rridExtensionId = id;
		}

		void SetAbsSendTimeExtensionId(uint8_t id)
		{
			this->absSendTimeExtensionId = id;
		}

		void SetTransportWideCc01ExtensionId(uint8_t id)
		{
			this->transportWideCc01ExtensionId = id;
		}

		// NOTE: Remove once RFC.
		void SetFrameMarking07ExtensionId(uint8_t id)
		{
			this->frameMarking07ExtensionId = id;
		}

		void SetFrameMarkingExtensionId(uint8_t id)
		{
			this->frameMarkingExtensionId = id;
		}

		void SetSsrcAudioLevelExtensionId(uint8_t id)
		{
			this->ssrcAudioLevelExtensionId = id;
		}

		void SetVideoOrientationExtensionId(uint8_t id)
		{
			this->videoOrientationExtensionId = id;
		}

		bool ReadMid(std::string& mid) const
		{
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->midExtensionId, extenLen);

			if (!extenValue || extenLen == 0u)
			{
				return false;
			}

			mid.assign(reinterpret_cast<const char*>(extenValue), static_cast<size_t>(extenLen));

			return true;
		}

		void UpdateMid(const std::string& mid);

		bool ReadRid(std::string& rid) const
		{
			// First try with the RID id then with the Repaired RID id.
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->ridExtensionId, extenLen);

			if (extenValue && extenLen > 0u)
			{
				rid.assign(reinterpret_cast<const char*>(extenValue), static_cast<size_t>(extenLen));

				return true;
			}

			extenValue = GetExtension(this->rridExtensionId, extenLen);

			if (extenValue && extenLen > 0u)
			{
				rid.assign(reinterpret_cast<const char*>(extenValue), static_cast<size_t>(extenLen));

				return true;
			}

			return false;
		}

		bool ReadAbsSendTime(uint32_t& absSendtime) const
		{
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->absSendTimeExtensionId, extenLen);

			if (!extenValue || extenLen != 3u)
			{
				return false;
			}

			absSendtime = Utils::Byte::Get3Bytes(extenValue, 0);

			return true;
		}

		bool UpdateAbsSendTime(uint64_t ms) const
		{
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->absSendTimeExtensionId, extenLen);

			if (!extenValue || extenLen != 3u)
			{
				return false;
			}

			auto absSendTime = Utils::Time::TimeMsToAbsSendTime(ms);

			Utils::Byte::Set3Bytes(extenValue, 0, absSendTime);

			return true;
		}

		bool ReadTransportWideCc01(uint16_t& wideSeqNumber) const
		{
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->transportWideCc01ExtensionId, extenLen);

			if (!extenValue || extenLen != 2u)
			{
				return false;
			}

			wideSeqNumber = Utils::Byte::Get2Bytes(extenValue, 0);

			return true;
		}

		bool UpdateTransportWideCc01(uint16_t wideSeqNumber) const
		{
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->transportWideCc01ExtensionId, extenLen);

			if (!extenValue || extenLen != 2u)
			{
				return false;
			}

			Utils::Byte::Set2Bytes(extenValue, 0, wideSeqNumber);

			return true;
		}

		bool ReadFrameMarking(RtpPacket::FrameMarking** frameMarking, uint8_t& length) const
		{
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->frameMarkingExtensionId, extenLen);

			// NOTE: Remove this once framemarking draft becomes RFC.
			if (!extenValue)
			{
				extenValue = GetExtension(this->frameMarking07ExtensionId, extenLen);
			}

			if (!extenValue || extenLen > 3u)
			{
				return false;
			}

			*frameMarking = reinterpret_cast<RtpPacket::FrameMarking*>(extenValue);
			length        = extenLen;

			return true;
		}

		bool ReadSsrcAudioLevel(uint8_t& volume, bool& voice) const
		{
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->ssrcAudioLevelExtensionId, extenLen);

			if (!extenValue || extenLen != 1u)
			{
				return false;
			}

			volume = Utils::Byte::Get1Byte(extenValue, 0);
			voice  = (volume & (1 << 7)) != 0;
			volume &= ~(1 << 7);

			return true;
		}

		bool ReadVideoOrientation(bool& camera, bool& flip, uint16_t& rotation) const
		{
			uint8_t extenLen;
			uint8_t* extenValue = GetExtension(this->videoOrientationExtensionId, extenLen);

			if (!extenValue || extenLen != 1u)
			{
				return false;
			}

			const uint8_t cvoByte       = Utils::Byte::Get1Byte(extenValue, 0);
			const uint8_t cameraValue   = ((cvoByte & 0b00001000) >> 3);
			const uint8_t flipValue     = ((cvoByte & 0b00000100) >> 2);
			const uint8_t rotationValue = (cvoByte & 0b00000011);

			camera = cameraValue != 0;
			flip   = flipValue != 0;

			// Using counter clockwise values.
			switch (rotationValue)
			{
				case 3:
					rotation = 270;
					break;
				case 2:
					rotation = 180;
					break;
				case 1:
					rotation = 90;
					break;
				default:
					rotation = 0;
			}

			return true;
		}

		bool HasExtension(uint8_t id) const
		{
			if (id == 0u)
			{
				return false;
			}
			else if (HasOneByteExtensions())
			{
				if (id > 14)
				{
					return false;
				}

				// `-1` because we have 14 elements total 0..13 and `id` is in the range 1..14.
				return this->oneByteExtensions[id - 1] != nullptr;
			}
			else if (HasTwoBytesExtensions())
			{
				auto it = this->mapTwoBytesExtensions.find(id);

				if (it == this->mapTwoBytesExtensions.end())
				{
					return false;
				}

				auto* extension = it->second;

				// In Two-Byte extensions value length may be zero. If so, return false.
				return extension->len != 0u;
			}
			else
			{
				return false;
			}
		}

		uint8_t* GetExtension(uint8_t id, uint8_t& len) const
		{
			len = 0u;

			if (id == 0u)
			{
				return nullptr;
			}
			else if (HasOneByteExtensions())
			{
				if (id > 14)
				{
					return nullptr;
				}

				// `-1` because we have 14 elements total 0..13 and `id` is in the range 1..14.
				auto* extension = this->oneByteExtensions[id - 1];

				if (!extension)
				{
					return nullptr;
				}

				// In One-Byte extensions value length 0 means 1.
				len = extension->len + 1;

				return extension->value;
			}
			else if (HasTwoBytesExtensions())
			{
				auto it = this->mapTwoBytesExtensions.find(id);

				if (it == this->mapTwoBytesExtensions.end())
				{
					return nullptr;
				}

				auto* extension = it->second;

				len = extension->len;

				// In Two-Byte extensions value length may be zero. If so, return nullptr.
				if (extension->len == 0u)
				{
					return nullptr;
				}

				return extension->value;
			}
			else
			{
				return nullptr;
			}
		}

		bool SetExtensionLength(uint8_t id, uint8_t len);

		uint8_t* GetPayload() const
		{
			return this->payloadLength != 0u ? this->payload : nullptr;
		}

		size_t GetPayloadLength() const
		{
			return this->payloadLength;
		}

		void SetPayloadLength(size_t length);

		uint8_t GetPayloadPadding() const
		{
			return this->payloadPadding;
		}

		uint8_t GetSpatialLayer() const
		{
			if (!this->payloadDescriptorHandler)
			{
				return 0u;
			}

			return this->payloadDescriptorHandler->GetSpatialLayer();
		}

		uint8_t GetTemporalLayer() const
		{
			if (!this->payloadDescriptorHandler)
			{
				return 0u;
			}

			return this->payloadDescriptorHandler->GetTemporalLayer();
		}

		bool IsKeyFrame() const
		{
			if (!this->payloadDescriptorHandler)
			{
				return false;
			}

			return this->payloadDescriptorHandler->IsKeyFrame();
		}

		RtpPacket* Clone() const;

		void RtxEncode(uint8_t payloadType, uint32_t ssrc, uint16_t seq);

		bool RtxDecode(uint8_t payloadType, uint32_t ssrc);

		void SetPayloadDescriptorHandler(const std::shared_ptr<Codecs::PayloadDescriptorHandler>& payloadDescriptorHandler)
		{
			this->payloadDescriptorHandler = payloadDescriptorHandler;
		}
        
        std::shared_ptr<const Codecs::PayloadDescriptorHandler> GetPayloadDescriptorHandler() const
        {
            return this->payloadDescriptorHandler;
        }

		bool ProcessPayload(RTC::Codecs::EncodingContext* context, bool& marker) const;

		void RestorePayload();

		void ShiftPayload(size_t payloadOffset, size_t shift, bool expand = true);

#ifdef MS_RTC_LOGGER_RTP
	public:
		RtcLogger::RtpPacket logger;
#endif

	private:
		void ParseExtensions();
        static size_t GetHeaderExtensionLength(HeaderExtension* headerExtension);

	private:
        // Buffer where this packet is allocated, can be `nullptr` if packet was
        // parsed from externally provided buffer.
        std::shared_ptr<Buffer> buffer;
		// Passed by argument.
        RtpPacketHeader* header{ nullptr };
		uint8_t* csrcList{ nullptr };
		HeaderExtension* headerExtension{ nullptr };
		// There might be up to 14 one-byte header extensions
		// (https://datatracker.ietf.org/doc/html/rfc5285#section-4.2), use std::array.
		std::array<OneByteExtension*, 14> oneByteExtensions{};
		absl::flat_hash_map<uint8_t, TwoBytesExtension*> mapTwoBytesExtensions;
		uint8_t midExtensionId{ 0u };
		uint8_t ridExtensionId{ 0u };
		uint8_t rridExtensionId{ 0u };
		uint8_t absSendTimeExtensionId{ 0u };
		uint8_t transportWideCc01ExtensionId{ 0u };
		uint8_t frameMarking07ExtensionId{ 0u }; // NOTE: Remove once RFC.
		uint8_t frameMarkingExtensionId{ 0u };
		uint8_t ssrcAudioLevelExtensionId{ 0u };
		uint8_t videoOrientationExtensionId{ 0u };
		uint8_t* payload{ nullptr };
		size_t payloadLength{ 0u };
		uint8_t payloadPadding{ 0u };
		size_t size{ 0u }; // Full size of the packet in bytes.
        // used in [Clone] method
        std::shared_ptr<BufferAllocator> allocator;
		// Codecs
		std::shared_ptr<Codecs::PayloadDescriptorHandler> payloadDescriptorHandler;
        std::unordered_set<Consumer*> acceptedConsumers;
	};
} // namespace RTC

#endif
