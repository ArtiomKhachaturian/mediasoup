#ifndef MS_RTC_CODECS_TOOLS_HPP
#define MS_RTC_CODECS_TOOLS_HPP

#include "common.hpp"
#include "RTC/Codecs/H264.hpp"
#include "RTC/Codecs/H264_SVC.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/Codecs/PayloadDescriptorHandler.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"

namespace RTC
{
	namespace Codecs
	{
		class Tools
		{
		public:
			static bool CanBeKeyFrame(const RTC::RtpCodecMimeType& mimeType)
			{
				switch (mimeType.GetType())
				{
					case RTC::RtpCodecMimeType::Type::VIDEO:
					{
						switch (mimeType.GetSubtype())
						{
							case RTC::RtpCodecMimeType::Subtype::VP8:
							case RTC::RtpCodecMimeType::Subtype::VP9:
							case RTC::RtpCodecMimeType::Subtype::H264:
							case RTC::RtpCodecMimeType::Subtype::H264_SVC:
								return true;
							default:
								return false;
						}
					}

					default:
					{
						return false;
					}
				}
			}

			static void ProcessRtpPacket(RTC::RtpPacket* packet, const RTC::RtpCodecMimeType& mimeType)
			{
				switch (mimeType.GetType())
				{
					case RTC::RtpCodecMimeType::Type::VIDEO:
					{
						switch (mimeType.GetSubtype())
						{
							case RTC::RtpCodecMimeType::Subtype::VP8:
							{
								RTC::Codecs::VP8::ProcessRtpPacket(packet);

								break;
							}

							case RTC::RtpCodecMimeType::Subtype::VP9:
							{
								RTC::Codecs::VP9::ProcessRtpPacket(packet);

								break;
							}

							case RTC::RtpCodecMimeType::Subtype::H264:
							{
								RTC::Codecs::H264::ProcessRtpPacket(packet);

								break;
							}
							case RTC::RtpCodecMimeType::Subtype::H264_SVC:
							{
								RTC::Codecs::H264_SVC::ProcessRtpPacket(packet);

								break;
							}

							default:;
						}
					}

					case RTC::RtpCodecMimeType::Type::AUDIO:
					{
						switch (mimeType.GetSubtype())
						{
							case RTC::RtpCodecMimeType::Subtype::OPUS:
							case RTC::RtpCodecMimeType::Subtype::MULTIOPUS:
							{
								RTC::Codecs::Opus::ProcessRtpPacket(packet);

								break;
							}

							default:;
						}
					}

					default:;
				}
			}

			static bool IsValidTypeForCodec(RTC::RtpParameters::Type type, const RTC::RtpCodecMimeType& mimeType)
			{
				switch (type)
				{
					case RTC::RtpParameters::Type::SIMPLE:
					{
						return true;
					}

					case RTC::RtpParameters::Type::SIMULCAST:
					{
						switch (mimeType.GetType())
						{
							case RTC::RtpCodecMimeType::Type::VIDEO:
							{
								switch (mimeType.GetSubtype())
								{
									case RTC::RtpCodecMimeType::Subtype::VP8:
									case RTC::RtpCodecMimeType::Subtype::H264:
										return true;
									default:
										return false;
								}
							}

							default:
							{
								return false;
							}
						}
					}

					case RTC::RtpParameters::Type::SVC:
					{
						switch (mimeType.GetType())
						{
							case RTC::RtpCodecMimeType::Type::VIDEO:
							{
								switch (mimeType.GetSubtype())
								{
									case RTC::RtpCodecMimeType::Subtype::VP9:
									case RTC::RtpCodecMimeType::Subtype::H264_SVC:
										return true;
									default:
										return false;
								}
							}

							default:
							{
								return false;
							}
						}
					}

					case RTC::RtpParameters::Type::PIPE:
					{
						return true;
					}
				}
			}

			static std::unique_ptr<EncodingContext> GetEncodingContext(
			  const RTC::RtpCodecMimeType& mimeType, const RTC::Codecs::EncodingContext::Params& params)
			{
				switch (mimeType.GetType())
				{
					case RTC::RtpCodecMimeType::Type::VIDEO:
					{
						switch (mimeType.GetSubtype())
						{
							case RTC::RtpCodecMimeType::Subtype::VP8:
								return std::make_unique<RTC::Codecs::VP8::EncodingContext>(params);
							case RTC::RtpCodecMimeType::Subtype::VP9:
								return std::make_unique<RTC::Codecs::VP9::EncodingContext>(params);
							case RTC::RtpCodecMimeType::Subtype::H264:
							case RTC::RtpCodecMimeType::Subtype::H264_SVC:
                                return std::make_unique<RTC::Codecs::EncodingContext>(params);
							default:
								return nullptr;
						}
					}

					case RTC::RtpCodecMimeType::Type::AUDIO:
					{
						switch (mimeType.GetSubtype())
						{
							case RTC::RtpCodecMimeType::Subtype::OPUS:
							case RTC::RtpCodecMimeType::Subtype::MULTIOPUS:
								return std::make_unique<RTC::Codecs::EncodingContext>(params);
							default:
								return nullptr;
						}
					}

					default:
					{
						return nullptr;
					}
				}
			}
		};
	} // namespace Codecs
} // namespace RTC

#endif
