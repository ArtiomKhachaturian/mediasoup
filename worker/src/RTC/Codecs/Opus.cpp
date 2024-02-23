#define MS_CLASS "RTC::Codecs::Opus"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Codecs/Opus.hpp"
#include "Logger.hpp"

namespace {

using namespace RTC::Codecs;

struct OpusPreset
{
    Opus::Mode _mode;
    Opus::Bandwitdh _bandwidth;
    Opus::FrameSize _frameSize;
};

// https://www.rfc-editor.org/rfc/rfc6716#section-3.1
static std::vector<OpusPreset> presets =
{
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::Narrowband    , Opus::FrameSize::ms10},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::Narrowband    , Opus::FrameSize::ms20},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::Narrowband    , Opus::FrameSize::ms40},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::Narrowband    , Opus::FrameSize::ms60},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::MediumBand    , Opus::FrameSize::ms10},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::MediumBand    , Opus::FrameSize::ms20},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::MediumBand    , Opus::FrameSize::ms40},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::MediumBand    , Opus::FrameSize::ms60},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::WideBand      , Opus::FrameSize::ms10},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::WideBand      , Opus::FrameSize::ms20},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::WideBand      , Opus::FrameSize::ms40},
    {Opus::Mode::SILKOnly    , Opus::Bandwitdh::WideBand      , Opus::FrameSize::ms60},
    {Opus::Mode::Hybrid      , Opus::Bandwitdh::SuperWideBand , Opus::FrameSize::ms10},
    {Opus::Mode::Hybrid      , Opus::Bandwitdh::SuperWideBand , Opus::FrameSize::ms20},
    {Opus::Mode::Hybrid      , Opus::Bandwitdh::FullBand      , Opus::FrameSize::ms10},
    {Opus::Mode::Hybrid      , Opus::Bandwitdh::FullBand      , Opus::FrameSize::ms20},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::Narrowband    , Opus::FrameSize::ms2_5},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::Narrowband    , Opus::FrameSize::ms5},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::Narrowband    , Opus::FrameSize::ms10},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::Narrowband    , Opus::FrameSize::ms20},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::WideBand      , Opus::FrameSize::ms2_5},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::WideBand      , Opus::FrameSize::ms5},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::WideBand      , Opus::FrameSize::ms10},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::WideBand      , Opus::FrameSize::ms20},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::SuperWideBand , Opus::FrameSize::ms2_5},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::SuperWideBand , Opus::FrameSize::ms5},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::SuperWideBand , Opus::FrameSize::ms10},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::SuperWideBand , Opus::FrameSize::ms20},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::FullBand      , Opus::FrameSize::ms2_5},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::FullBand      , Opus::FrameSize::ms5},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::FullBand      , Opus::FrameSize::ms10},
    {Opus::Mode::CELTOnly    , Opus::Bandwitdh::FullBand      , Opus::FrameSize::ms20},
};

}

namespace RTC
{
	namespace Codecs
	{
		/* Class methods. */

        std::unique_ptr<Opus::PayloadDescriptor> Opus::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

            auto payloadDescriptor = std::make_unique<Opus::PayloadDescriptor>();

			// libopus generates a single byte payload (TOC, no frames) to generate DTX.
			if (len == 1)
			{
				payloadDescriptor->isDtx = true;
			}
            payloadDescriptor->size = 1UL;
			return payloadDescriptor;
		}

		void Opus::ProcessRtpPacket(RTC::RtpPacket* packet)
		{
			MS_TRACE();

			auto* data = packet->GetPayload();
			auto len   = packet->GetPayloadLength();

            if (auto payloadDescriptor = Opus::Parse(data, len)) {
                packet->SetPayloadDescriptorHandler(std::make_shared<PayloadDescriptorHandler>(std::move(payloadDescriptor)));
            }			
		}
    
        Opus::OpusHead::OpusHead(uint8_t channelCount, uint32_t sampleRate)
            : _channelCount(channelCount)
            , _sampleRate(sampleRate)
        {
        }
    
        void Opus::ParseTOC(const uint8_t toc, Mode* mode,
                            Bandwitdh* bandWidth, FrameSize* frameSize,
                            bool* stereo, CodeNumber* codeNumber)
        {
            if (stereo) {
                *stereo = toc & 0x04;
            }
            if (codeNumber) {
                const uint8_t number = toc | 0x03;
                MS_ASSERT(number <= static_cast<uint8_t>(CodeNumber::Arbitrary), "OPUS TOC code number is invalid");
                *codeNumber = static_cast<CodeNumber>(number);
            }
            if (mode || bandWidth || frameSize) {
                // Get config
                const uint8_t config = toc >> 3;
                if (config < presets.size()) {
                    const auto& preset = presets[config];
                    if (mode) {
                        *mode = preset._mode;
                    }
                    if (bandWidth) {
                        *bandWidth = preset._bandwidth;
                    }
                    if (frameSize) {
                        *frameSize = preset._frameSize;
                    }
                }
                else {
                    MS_ASSERT(false, "OPUS TOC config is invalid");
                }
            }
        }
    
        void Opus::ParseTOC(const uint8_t* payload, Mode* mode,
                            Bandwitdh* bandWidth, FrameSize* frameSize,
                            bool* stereo, CodeNumber* codeNumber)
        {
            if (payload) {
                ParseTOC(payload[0], mode, bandWidth, frameSize, stereo, codeNumber);
            }
        }

		/* Instance methods. */

		void Opus::PayloadDescriptor::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<Opus::PayloadDescriptor>");
			MS_DUMP("  isDtx: %s", this->isDtx ? "true" : "false");
			MS_DUMP("</Opus::PayloadDescriptor>");
		}

		Opus::PayloadDescriptorHandler::PayloadDescriptorHandler(std::unique_ptr<PayloadDescriptor> payloadDescriptor)
		{
			MS_TRACE();

			this->payloadDescriptor = std::move(payloadDescriptor);
		}

		bool Opus::PayloadDescriptorHandler::Process(
		  RTC::Codecs::EncodingContext* encodingContext, uint8_t* data, bool& /*marker*/)
		{
			MS_TRACE();

			auto* context = static_cast<RTC::Codecs::Opus::EncodingContext*>(encodingContext);

			if (this->payloadDescriptor->isDtx && context->GetIgnoreDtx())
			{
				return false;
			}
			else
			{
				return true;
			}
		};
	} // namespace Codecs
} // namespace RTC
