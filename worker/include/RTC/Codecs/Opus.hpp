#ifndef MS_RTC_CODECS_OPUS_HPP
#define MS_RTC_CODECS_OPUS_HPP

#include "common.hpp"
#include "RTC/Codecs/PayloadDescriptorHandler.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/SeqManager.hpp"

namespace RTC
{
	namespace Codecs
	{
		class Opus
		{
		public:
			struct PayloadDescriptor : public RTC::Codecs::PayloadDescriptor
			{
				void Dump() const override;

				// Parsed values.
				bool isDtx{ false };
			};

		public:
			static std::unique_ptr<Opus::PayloadDescriptor> Parse(const uint8_t* data, size_t len);
			static void ProcessRtpPacket(RTC::RtpPacket* packet);

		public:
			class EncodingContext : public RTC::Codecs::EncodingContext
			{
			public:
				explicit EncodingContext(const RTC::Codecs::EncodingContext::Params& params)
				  : RTC::Codecs::EncodingContext(params)
				{
				}
				~EncodingContext() = default;

				/* Pure virtual methods inherited from RTC::Codecs::EncodingContext. */
			public:
				void SyncRequired() override
				{
					this->syncRequired = true;
				}

			public:
				bool syncRequired{ false };
			};

		public:
			class PayloadDescriptorHandler : public RTC::Codecs::PayloadDescriptorHandler
			{
			public:
				explicit PayloadDescriptorHandler(std::unique_ptr<PayloadDescriptor> payloadDescriptor);

			public:
				void Dump() const override
				{
					this->payloadDescriptor->Dump();
				}
				bool Process(RTC::Codecs::EncodingContext* encodingContext, uint8_t* data, bool& marker) override;
				void Restore(uint8_t* data) override
				{
					return;
				}
				uint8_t GetSpatialLayer() const override
				{
					return 0u;
				}
				uint8_t GetTemporalLayer() const override
				{
					return 0u;
				}
				bool IsKeyFrame() const override
				{
					return false;
				}
                size_t GetPayloadDescriptorSize() const override
                {
                    return this->payloadDescriptor->size;
                }

			private:
				std::unique_ptr<PayloadDescriptor> payloadDescriptor;
			};
            
        public:
#pragma pack(push)
#pragma pack(1)
            struct OpusHead
            {
                // constants
                static inline constexpr uint8_t _defaultChannelCount = 1U;
                static inline constexpr uint32_t _defaultSampleRate = 48000U;
                // members
                const uint8_t _head[8] = {0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64};
                const uint8_t _version = 1U;
                uint8_t _channelCount = _defaultChannelCount;
                const uint16_t _preSkip = 0U;
                uint32_t _sampleRate = _defaultSampleRate;
                const uint16_t _gain = 0U;
                const uint8_t _mappingFamily = 0U;
                OpusHead() = default;
                OpusHead(uint8_t channelCount, uint32_t sampleRate);
            };
#pragma pack(pop)
            
        public:
            enum class Mode
            {
                SILKOnly,
                Hybrid,
                CELTOnly
            };
            
            enum class CodeNumber
            {
                One = 0,
                Two = 1,
                Three = 2,
                Arbitrary = 3,
            };
            
            enum class Bandwitdh
            {
                Narrowband,
                MediumBand,
                WideBand,
                SuperWideBand,
                FullBand,
            };
            
            enum class FrameSize
            {
                ms2_5   = 120,
                ms5     = 240,
                ms10    = 480,
                ms20    = 960,
                ms40    = 1920,
                ms60    = 2880
            };
            
            static void ParseTOC(uint8_t toc, Mode* mode = nullptr,
                                 Bandwitdh* bandWidth = nullptr, FrameSize* frameSize = nullptr,
                                 bool* stereo = nullptr, CodeNumber* codeNumber = nullptr);
		};
	} // namespace Codecs
} // namespace RTC

#endif
