#define MS_CLASS "OutputWebSocketDevice"
#include "RTC/OutputWebSocketDevice.hpp"
#include <algorithm>
#include <cctype>
#include <iterator>
#include <stdexcept>
#include "Logger.hpp"

namespace {

// split by port & domain
inline void SplitPathToDomainAndPort(std::string_view path, std::string& domain, std::string& port)
{
    if (path.size() > 1UL) {
        // split by port & domain
        const auto pathSplitter = path.find(':');
        if (std::string::npos != pathSplitter) {
            domain.insert(domain.begin(), path.begin(), path.begin() + pathSplitter);
            port.insert(port.begin(), path.begin() + pathSplitter + 1UL, path.end());
        } else {
            domain = std::move(path);
        }
    }
}

} // namespace

namespace RTC
{

struct OutputWebSocketDevice::UriParts
{
    UriParts(const std::string& uri);
    static const std::string _wssScheme;
    static const std::string _wsScheme;
    bool _isConnectionSecure = false;
    std::string _domain;
    int _port = CONTEXT_PORT_NO_LISTEN;
    std::string _uri;
    static std::string CreateUriWithScheme(const std::string& baseUri, bool secure);
    bool isValid() const { return !_uri.empty(); }
};

OutputWebSocketDevice::OutputWebSocketDevice(const std::string& uri,
                                             std::unordered_map<std::string, std::string> extraHeaders)
    : _uriParts(std::make_unique<UriParts>(uri))
    , _extraHeaders(std::move(extraHeaders))
{
    if (_uriParts->isValid()) {
        // init protocols
        _protocols.reset(new lws_protocols[3]);
        std::memset(_protocols.get(), 0, sizeof(lws_protocols) * 3);
        
        struct lws_context_creation_info contextInfo = {};
        contextInfo.port = _uriParts->_port;
    }
}

OutputWebSocketDevice::OutputWebSocketDevice(const std::string& baseUri, bool secure,
                                             std::unordered_map<std::string, std::string> extraHeaders)
    : OutputWebSocketDevice(UriParts::CreateUriWithScheme(baseUri, secure), std::move(extraHeaders))
{
}

OutputWebSocketDevice::~OutputWebSocketDevice()
{
}

bool OutputWebSocketDevice::Write(const void* buf, uint32_t len)
{
    return false;
}

const std::string OutputWebSocketDevice::UriParts::_wssScheme("wss");
const std::string OutputWebSocketDevice::UriParts::_wsScheme("ws");

OutputWebSocketDevice::UriParts::UriParts(const std::string& uri)
{
    if (!uri.empty()) {
        auto protocolEnd = uri.find("://");
        // extract protocol (URI scheme)
        if (std::string::npos != protocolEnd) {
            std::string protocol;
            protocol.reserve(protocolEnd);
            std::transform(uri.begin(), uri.begin() + protocolEnd, std::back_inserter(protocol),
                           [](unsigned char c) { return std::tolower(c); });
            // is websocket proto?
            if (_wssScheme == protocol || _wsScheme == protocol) {
                protocolEnd += 3;
                _isConnectionSecure = _wssScheme == protocol; // secure or not
                // extract URI part
                const auto pathEnd = uri.find_first_of('/', protocolEnd);
                std::string port;
                if (std::string::npos != pathEnd) {
                    SplitPathToDomainAndPort(std::string(uri.begin() + protocolEnd, uri.begin() + pathEnd), _domain, port);
                } else {
                    SplitPathToDomainAndPort(std::string(uri.begin() + protocolEnd, uri.end()), _domain, port);
                }
                _uri = uri;
                if (!port.empty()) {
                    try {
                        _port = std::stoi(port.c_str());
                    }
                    catch(const std::exception& e) {
                        MS_WARN_TAG(rtp, "Failed to extract port number: %s", e.what());
                    }
                }
            }
        }
    }
}

std::string OutputWebSocketDevice::UriParts::CreateUriWithScheme(const std::string& baseUri, bool secure)
{
    if (!baseUri.empty()) {
        return (secure ? UriParts::_wssScheme : UriParts::_wsScheme) + "://" + baseUri;
    }
    return baseUri;
}


} // namespace RTC
