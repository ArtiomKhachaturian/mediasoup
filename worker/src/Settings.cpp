#define MS_CLASS "Settings"
// #define MS_LOG_DEV_LEVEL 3

#include "Settings.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include <flatbuffers/flatbuffers.h>
#include <cctype>   // isprint()
#include <iterator> // std::ostream_iterator
#include <mutex>
#include <sstream> // std::ostringstream
#include <stdexcept>
extern "C"
{
#include <getopt.h>
}

/* Static. */

static std::mutex GlobalSyncMutex;

/* Class variables. */

struct Settings::Configuration Settings::configuration;
// clang-format off
absl::flat_hash_map<std::string, LogLevel> Settings::String2LogLevel =
{
	{ "debug", LogLevel::LOG_DEBUG },
	{ "warn",  LogLevel::LOG_WARN  },
	{ "error", LogLevel::LOG_ERROR },
	{ "none",  LogLevel::LOG_NONE  }
};
absl::flat_hash_map<LogLevel, std::string> Settings::LogLevel2String =
{
	{ LogLevel::LOG_DEBUG, "debug" },
	{ LogLevel::LOG_WARN,  "warn"  },
	{ LogLevel::LOG_ERROR, "error" },
	{ LogLevel::LOG_NONE,  "none"  }
};
// clang-format on

Settings::LogTags::LogTags(const LogTags& other)
{
    this->info = other.info.load();
    this->ice = other.ice.load();
    this->dtls = other.dtls.load();
    this->rtp = other.rtp.load();
    this->srtp = other.srtp.load();
    this->rtcp = other.rtcp.load();
    this->rtx = other.rtx.load();
    this->bwe = other.bwe.load();
    this->score = other.score.load();
    this->simulcast = other.simulcast.load();
    this->svc = other.svc.load();
    this->sctp = other.sctp.load();
    this->message = other.message.load();
}

Settings::LogTags& Settings::LogTags::operator = (const LogTags& other)
{
    if (&other != this) {
        this->info = other.info.load();
        this->ice = other.ice.load();
        this->dtls = other.dtls.load();
        this->rtp = other.rtp.load();
        this->srtp = other.srtp.load();
        this->rtcp = other.rtcp.load();
        this->rtx = other.rtx.load();
        this->bwe = other.bwe.load();
        this->score = other.score.load();
        this->simulcast = other.simulcast.load();
        this->svc = other.svc.load();
        this->sctp = other.sctp.load();
        this->message = other.message.load();
    }
    return *this;
}

void Settings::Configuration::SetDtlsCertificateFile(std::string dtlsCertificateFile)
{
    LOCK_WRITE_PROTECTED_OBJ(this->dtlsCertificateFile);
    this->dtlsCertificateFile = std::move(dtlsCertificateFile);
}

std::string Settings::Configuration::GetDtlsCertificateFile() const
{
    LOCK_READ_PROTECTED_OBJ(this->dtlsCertificateFile);
    return this->dtlsCertificateFile.ConstRef();
}

void Settings::Configuration::SetDtlsPrivateKeyFile(std::string dtlsPrivateKeyFile)
{
    LOCK_WRITE_PROTECTED_OBJ(this->dtlsPrivateKeyFile);
    this->dtlsPrivateKeyFile = std::move(dtlsCertificateFile);
}

std::string Settings::Configuration::GetDtlsPrivateKeyFile() const
{
    LOCK_READ_PROTECTED_OBJ(this->dtlsPrivateKeyFile);
    return this->dtlsPrivateKeyFile.ConstRef();
}

void Settings::Configuration::SetLibwebrtcFieldTrials(std::string libwebrtcFieldTrials)
{
    LOCK_WRITE_PROTECTED_OBJ(this->libwebrtcFieldTrials);
    if (libwebrtcFieldTrials != this->libwebrtcFieldTrials.ConstRef()) {
        MS_WARN_TAG(info, "overriding default value of libwebrtcFieldTrials may generate crashes in mediasoup-worker");
        this->libwebrtcFieldTrials = std::move(libwebrtcFieldTrials);
    }
}

std::string Settings::Configuration::GetLibwebrtcFieldTrials() const
{
    LOCK_READ_PROTECTED_OBJ(this->libwebrtcFieldTrials);
    return this->libwebrtcFieldTrials.ConstRef();
}

/* Class methods. */

void Settings::SetConfiguration(int argc, char* argv[])
{
	MS_TRACE();

	/* Variables for getopt. */

	int c;
	int optionIdx{ 0 };
	// clang-format off
	struct option options[] =
	{
		{ "logLevel",             optional_argument, nullptr, 'l' },
		{ "logTags",              optional_argument, nullptr, 't' },
		{ "rtcMinPort",           optional_argument, nullptr, 'm' },
		{ "rtcMaxPort",           optional_argument, nullptr, 'M' },
		{ "dtlsCertificateFile",  optional_argument, nullptr, 'c' },
		{ "dtlsPrivateKeyFile",   optional_argument, nullptr, 'p' },
		{ "libwebrtcFieldTrials", optional_argument, nullptr, 'W' },
		{ nullptr, 0, nullptr, 0 }
	};
	// clang-format on
	std::string stringValue;
	std::vector<std::string> logTags;

	/* Parse command line options. */

	// getopt_long_only() is not thread-safe
	const std::lock_guard<std::mutex> lock(GlobalSyncMutex);

	optind = 1; // Set explicitly, otherwise subsequent runs will fail.
	opterr = 0; // Don't allow getopt to print error messages.
	while ((c = getopt_long_only(argc, argv, "", options, &optionIdx)) != -1)
	{
		if (!optarg)
		{
			MS_THROW_TYPE_ERROR("unknown configuration parameter: %s", optarg);
		}

		switch (c)
		{
			case 'l':
			{
				SetLogLevel(std::string(optarg));

				break;
			}

			case 't':
			{
				stringValue = std::string(optarg);
				logTags.push_back(stringValue);

				break;
			}

			case 'm':
			{
				try
				{
					Settings::configuration.rtcMinPort = static_cast<uint16_t>(std::stoi(optarg));
				}
				catch (const std::exception& error)
				{
					MS_THROW_TYPE_ERROR("%s", error.what());
				}

				break;
			}

			case 'M':
			{
				try
				{
					Settings::configuration.rtcMaxPort = static_cast<uint16_t>(std::stoi(optarg));
				}
				catch (const std::exception& error)
				{
					MS_THROW_TYPE_ERROR("%s", error.what());
				}

				break;
			}

			case 'c':
			{
                Settings::configuration.SetDtlsCertificateFile(std::string(optarg));
				break;
			}

			case 'p':
			{
                Settings::configuration.SetDtlsPrivateKeyFile(std::string(optarg));
				break;
			}

			case 'W':
			{
                Settings::configuration.SetLibwebrtcFieldTrials(std::string(optarg));
				break;
			}

			// Invalid option.
			case '?':
			{
				if (isprint(optopt) != 0)
				{
					MS_THROW_TYPE_ERROR("invalid option '-%c'", (char)optopt);
				}
				else
				{
					MS_THROW_TYPE_ERROR("unknown long option given as argument");
				}
			}

			// Valid option, but it requires and argument that is not given.
			case ':':
			{
				MS_THROW_TYPE_ERROR("option '%c' requires an argument", (char)optopt);
			}

			// This should never happen.
			default:
			{
				MS_THROW_TYPE_ERROR("'default' should never happen");
			}
		}
	}

	/* Post configuration. */

	// Set logTags.
	if (!logTags.empty())
	{
		Settings::SetLogTags(logTags);
	}

	// Validate RTC ports.
	if (Settings::configuration.rtcMaxPort < Settings::configuration.rtcMinPort)
	{
		MS_THROW_TYPE_ERROR("rtcMaxPort cannot be less than rtcMinPort");
	}

	// Set DTLS certificate files (if provided),
	Settings::SetDtlsCertificateAndPrivateKeyFiles();
}

void Settings::PrintConfiguration()
{
	MS_TRACE();

	std::vector<std::string> logTags;
	std::ostringstream logTagsStream;

	if (Settings::configuration.logTags.info)
	{
		logTags.emplace_back("info");
	}
	if (Settings::configuration.logTags.ice)
	{
		logTags.emplace_back("ice");
	}
	if (Settings::configuration.logTags.dtls)
	{
		logTags.emplace_back("dtls");
	}
	if (Settings::configuration.logTags.rtp)
	{
		logTags.emplace_back("rtp");
	}
	if (Settings::configuration.logTags.srtp)
	{
		logTags.emplace_back("srtp");
	}
	if (Settings::configuration.logTags.rtcp)
	{
		logTags.emplace_back("rtcp");
	}
	if (Settings::configuration.logTags.rtx)
	{
		logTags.emplace_back("rtx");
	}
	if (Settings::configuration.logTags.bwe)
	{
		logTags.emplace_back("bwe");
	}
	if (Settings::configuration.logTags.score)
	{
		logTags.emplace_back("score");
	}
	if (Settings::configuration.logTags.simulcast)
	{
		logTags.emplace_back("simulcast");
	}
	if (Settings::configuration.logTags.svc)
	{
		logTags.emplace_back("svc");
	}
	if (Settings::configuration.logTags.sctp)
	{
		logTags.emplace_back("sctp");
	}
	if (Settings::configuration.logTags.message)
	{
		logTags.emplace_back("message");
	}

	if (!logTags.empty())
	{
		std::copy(
		  logTags.begin(), logTags.end() - 1, std::ostream_iterator<std::string>(logTagsStream, ","));
		logTagsStream << logTags.back();
	}

	MS_DEBUG_TAG(info, "<configuration>");

	MS_DEBUG_TAG(info, "  logLevel: %s", Settings::LogLevel2String[Settings::configuration.logLevel].c_str());
	MS_DEBUG_TAG(info, "  logTags: %s", logTagsStream.str().c_str());
	MS_DEBUG_TAG(info, "  rtcMinPort: %" PRIu16, Settings::configuration.rtcMinPort.load());
	MS_DEBUG_TAG(info, "  rtcMaxPort: %" PRIu16, Settings::configuration.rtcMaxPort.load());
    
    const auto dtlsCertificateFile = Settings::configuration.GetDtlsCertificateFile();
    if (!dtlsCertificateFile.empty())
    {
        MS_DEBUG_TAG(info, "  dtlsCertificateFile: %s", dtlsCertificateFile.c_str());
        MS_DEBUG_TAG(info, "  dtlsPrivateKeyFile: %s", Settings::configuration.GetDtlsPrivateKeyFile().c_str());
    }

    const auto libwebrtcFieldTrials = Settings::configuration.GetLibwebrtcFieldTrials();
    if (!libwebrtcFieldTrials.empty())
	{
		MS_DEBUG_TAG(info, "  libwebrtcFieldTrials: %s", libwebrtcFieldTrials.c_str());
	}

	MS_DEBUG_TAG(info, "</configuration>");
}

void Settings::HandleRequest(Channel::ChannelRequest* request)
{
	MS_TRACE();

	switch (request->method)
	{
		case Channel::ChannelRequest::Method::WORKER_UPDATE_SETTINGS:
		{
			const auto* body = request->data->body_as<FBS::Worker::UpdateSettingsRequest>();

			if (flatbuffers::IsFieldPresent(body, FBS::Worker::UpdateSettingsRequest::VT_LOGLEVEL))
			{
				// This may throw.
				Settings::SetLogLevel(body->logLevel()->str());
			}

			// Update logTags if requested.
			if (flatbuffers::IsFieldPresent(body, FBS::Worker::UpdateSettingsRequest::VT_LOGTAGS))
			{
				std::vector<std::string> logTags;

				for (const auto& logTag : *body->logTags())
				{
					logTags.push_back(logTag->str());
				}

				Settings::SetLogTags(logTags);
			}

			// Print the new effective configuration.
			Settings::PrintConfiguration();

			request->Accept();

			break;
		}

		default:
		{
			MS_THROW_ERROR("unknown method '%s'", request->methodCStr);
		}
	}
}

void Settings::SetLogLevel(std::string level)
{
	MS_TRACE();

	// Lowcase given level.
	Utils::String::ToLowerCase(level);

	if (Settings::String2LogLevel.find(level) == Settings::String2LogLevel.end())
	{
		MS_THROW_TYPE_ERROR("invalid value '%s' for logLevel", level.c_str());
	}

	Settings::configuration.logLevel = Settings::String2LogLevel[level];
}

void Settings::SetLogTags(const std::vector<std::string>& tags)
{
	MS_TRACE();

	// Reset logTags.
	struct LogTags newLogTags;

	for (const auto& tag : tags)
	{
		if (tag == "info")
		{
			newLogTags.info = true;
		}
		else if (tag == "ice")
		{
			newLogTags.ice = true;
		}
		else if (tag == "dtls")
		{
			newLogTags.dtls = true;
		}
		else if (tag == "rtp")
		{
			newLogTags.rtp = true;
		}
		else if (tag == "srtp")
		{
			newLogTags.srtp = true;
		}
		else if (tag == "rtcp")
		{
			newLogTags.rtcp = true;
		}
		else if (tag == "rtx")
		{
			newLogTags.rtx = true;
		}
		else if (tag == "bwe")
		{
			newLogTags.bwe = true;
		}
		else if (tag == "score")
		{
			newLogTags.score = true;
		}
		else if (tag == "simulcast")
		{
			newLogTags.simulcast = true;
		}
		else if (tag == "svc")
		{
			newLogTags.svc = true;
		}
		else if (tag == "sctp")
		{
			newLogTags.sctp = true;
		}
		else if (tag == "message")
		{
			newLogTags.message = true;
		}
	}

	Settings::configuration.logTags = newLogTags;
}

void Settings::SetDtlsCertificateAndPrivateKeyFiles()
{
	MS_TRACE();
    
    const auto dtlsCertificateFile = Settings::configuration.GetDtlsCertificateFile();
    const auto dtlsPrivateKeyFile = Settings::configuration.GetDtlsPrivateKeyFile();
	
    if (!dtlsCertificateFile.empty() && dtlsPrivateKeyFile.empty())
	{
		MS_THROW_TYPE_ERROR("missing dtlsPrivateKeyFile");
	}
	else if (dtlsCertificateFile.empty() && !dtlsPrivateKeyFile.empty())
	{
		MS_THROW_TYPE_ERROR("missing dtlsCertificateFile");
	}
	else if (dtlsCertificateFile.empty() && dtlsPrivateKeyFile.empty())
	{
		return;
	}

	try
	{
		Utils::File::CheckFile(dtlsCertificateFile.c_str());
	}
	catch (const MediaSoupError& error)
	{
		MS_THROW_TYPE_ERROR("dtlsCertificateFile: %s", error.what());
	}

	try
	{
		Utils::File::CheckFile(dtlsPrivateKeyFile.c_str());
	}
	catch (const MediaSoupError& error)
	{
		MS_THROW_TYPE_ERROR("dtlsPrivateKeyFile: %s", error.what());
	}
}
