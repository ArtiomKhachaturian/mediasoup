#ifndef MS_SETTINGS_HPP
#define MS_SETTINGS_HPP

#include "common.hpp"
#include "LogLevel.hpp"
#include "ProtectedObj.hpp"
#include "Channel/ChannelRequest.hpp"
#include <absl/container/flat_hash_map.h>
#include <atomic>
#include <string>
#include <vector>

class Settings
{
public:
	struct LogTags
	{
        LogTags() = default;
        LogTags(const LogTags& other);
        LogTags(LogTags&&) = delete;
		std::atomic_bool info{ false };
        std::atomic_bool ice{ false };
        std::atomic_bool dtls{ false };
        std::atomic_bool rtp{ false };
        std::atomic_bool srtp{ false };
        std::atomic_bool rtcp{ false };
        std::atomic_bool rtx{ false };
        std::atomic_bool bwe{ false };
        std::atomic_bool score{ false };
        std::atomic_bool simulcast{ false };
        std::atomic_bool svc{ false };
        std::atomic_bool sctp{ false };
        std::atomic_bool message{ false };
        LogTags& operator = (const LogTags& other);
        LogTags& operator = (LogTags&&) = delete;
	};

public:
	// Struct holding the configuration.
	struct Configuration
	{
		std::atomic<LogLevel> logLevel{ LogLevel::LOG_ERROR };
		struct LogTags logTags;
        std::atomic<uint16_t> rtcMinPort{ 10000u };
        std::atomic<uint16_t> rtcMaxPort{ 59999u };
        void SetDtlsCertificateFile(std::string dtlsCertificateFile);
        std::string GetDtlsCertificateFile() const;
        void SetDtlsPrivateKeyFile(std::string dtlsPrivateKeyFile);
        std::string GetDtlsPrivateKeyFile() const;
        void SetLibwebrtcFieldTrials(std::string libwebrtcFieldTrials);
        std::string GetLibwebrtcFieldTrials() const;
    private:
		RTC::ProtectedObj<std::string> dtlsCertificateFile;
        RTC::ProtectedObj<std::string> dtlsPrivateKeyFile;
        RTC::ProtectedObj<std::string> libwebrtcFieldTrials{ "WebRTC-Bwe-AlrLimitedBackoff/Enabled/" };
	};

public:
	static void SetConfiguration(int argc, char* argv[]);
	static void PrintConfiguration();
	static void HandleRequest(Channel::ChannelRequest* request);

private:
	static void SetLogLevel(std::string level);
	static void SetLogTags(const std::vector<std::string>& tags);
	static void SetDtlsCertificateAndPrivateKeyFiles();

public:
	static struct Configuration configuration;

private:
	static absl::flat_hash_map<std::string, LogLevel> String2LogLevel; // NOLINT(readability-identifier-naming)
	static absl::flat_hash_map<LogLevel, std::string> LogLevel2String; // NOLINT(readability-identifier-naming)
};

#endif
