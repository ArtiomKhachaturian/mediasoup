#include "RTC/MediaTranslate/Websocket/WebsocketTppFactory.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTpp.hpp"
#ifdef LOCAL_WEBSOCKET_TEST_SERVER
#define MS_CLASS "RTC::WebsocketTppFactory::TestServer"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTppUtils.hpp"
#include "Logger.hpp"
#include "ProtectedObj.hpp"
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <set>
#include <map>
#include <thread>
#endif

#ifdef LOCAL_WEBSOCKET_TEST_SERVER
namespace {

using Server = websocketpp::server<websocketpp::config::asio_tls>;
using MessagePtr = websocketpp::config::asio::message_type::ptr;

const std::string g_cert = "-----BEGIN CERTIFICATE-----\n"
                           "MIIDaDCCAlCgAwIBAgIJAO8vBu8i8exWMA0GCSqGSIb3DQEBCwUAMEkxCzAJBgNV\n"
                           "BAYTAlVTMQswCQYDVQQIDAJDQTEtMCsGA1UEBwwkTG9zIEFuZ2VsZXNPPUJlYXN0\n"
                           "Q049d3d3LmV4YW1wbGUuY29tMB4XDTE3MDUwMzE4MzkxMloXDTQ0MDkxODE4Mzkx\n"
                           "MlowSTELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAkNBMS0wKwYDVQQHDCRMb3MgQW5n\n"
                           "ZWxlc089QmVhc3RDTj13d3cuZXhhbXBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUA\n"
                           "A4IBDwAwggEKAoIBAQDJ7BRKFO8fqmsEXw8v9YOVXyrQVsVbjSSGEs4Vzs4cJgcF\n"
                           "xqGitbnLIrOgiJpRAPLy5MNcAXE1strVGfdEf7xMYSZ/4wOrxUyVw/Ltgsft8m7b\n"
                           "Fu8TsCzO6XrxpnVtWk506YZ7ToTa5UjHfBi2+pWTxbpN12UhiZNUcrRsqTFW+6fO\n"
                           "9d7xm5wlaZG8cMdg0cO1bhkz45JSl3wWKIES7t3EfKePZbNlQ5hPy7Pd5JTmdGBp\n"
                           "yY8anC8u4LPbmgW0/U31PH0rRVfGcBbZsAoQw5Tc5dnb6N2GEIbq3ehSfdDHGnrv\n"
                           "enu2tOK9Qx6GEzXh3sekZkxcgh+NlIxCNxu//Dk9AgMBAAGjUzBRMB0GA1UdDgQW\n"
                           "BBTZh0N9Ne1OD7GBGJYz4PNESHuXezAfBgNVHSMEGDAWgBTZh0N9Ne1OD7GBGJYz\n"
                           "4PNESHuXezAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQCmTJVT\n"
                           "LH5Cru1vXtzb3N9dyolcVH82xFVwPewArchgq+CEkajOU9bnzCqvhM4CryBb4cUs\n"
                           "gqXWp85hAh55uBOqXb2yyESEleMCJEiVTwm/m26FdONvEGptsiCmF5Gxi0YRtn8N\n"
                           "V+KhrQaAyLrLdPYI7TrwAOisq2I1cD0mt+xgwuv/654Rl3IhOMx+fKWKJ9qLAiaE\n"
                           "fQyshjlPP9mYVxWOxqctUdQ8UnsUKKGEUcVrA08i1OAnVKlPFjKBvk+r7jpsTPcr\n"
                           "9pWXTO9JrYMML7d+XRSZA1n3856OqZDX4403+9FnXCvfcLZLLKTBvwwFgEFGpzjK\n"
                           "UEVbkhd5qstF6qWK\n"
                           "-----END CERTIFICATE-----\n";

const std::string g_certKey = "-----BEGIN PRIVATE KEY-----\n"
                              "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDJ7BRKFO8fqmsE\n"
                              "Xw8v9YOVXyrQVsVbjSSGEs4Vzs4cJgcFxqGitbnLIrOgiJpRAPLy5MNcAXE1strV\n"
                              "GfdEf7xMYSZ/4wOrxUyVw/Ltgsft8m7bFu8TsCzO6XrxpnVtWk506YZ7ToTa5UjH\n"
                              "fBi2+pWTxbpN12UhiZNUcrRsqTFW+6fO9d7xm5wlaZG8cMdg0cO1bhkz45JSl3wW\n"
                              "KIES7t3EfKePZbNlQ5hPy7Pd5JTmdGBpyY8anC8u4LPbmgW0/U31PH0rRVfGcBbZ\n"
                              "sAoQw5Tc5dnb6N2GEIbq3ehSfdDHGnrvenu2tOK9Qx6GEzXh3sekZkxcgh+NlIxC\n"
                              "Nxu//Dk9AgMBAAECggEBAK1gV8uETg4SdfE67f9v/5uyK0DYQH1ro4C7hNiUycTB\n"
                              "oiYDd6YOA4m4MiQVJuuGtRR5+IR3eI1zFRMFSJs4UqYChNwqQGys7CVsKpplQOW+\n"
                              "1BCqkH2HN/Ix5662Dv3mHJemLCKUON77IJKoq0/xuZ04mc9csykox6grFWB3pjXY\n"
                              "OEn9U8pt5KNldWfpfAZ7xu9WfyvthGXlhfwKEetOuHfAQv7FF6s25UIEU6Hmnwp9\n"
                              "VmYp2twfMGdztz/gfFjKOGxf92RG+FMSkyAPq/vhyB7oQWxa+vdBn6BSdsfn27Qs\n"
                              "bTvXrGe4FYcbuw4WkAKTljZX7TUegkXiwFoSps0jegECgYEA7o5AcRTZVUmmSs8W\n"
                              "PUHn89UEuDAMFVk7grG1bg8exLQSpugCykcqXt1WNrqB7x6nB+dbVANWNhSmhgCg\n"
                              "VrV941vbx8ketqZ9YInSbGPWIU/tss3r8Yx2Ct3mQpvpGC6iGHzEc/NHJP8Efvh/\n"
                              "CcUWmLjLGJYYeP5oNu5cncC3fXUCgYEA2LANATm0A6sFVGe3sSLO9un1brA4zlZE\n"
                              "Hjd3KOZnMPt73B426qUOcw5B2wIS8GJsUES0P94pKg83oyzmoUV9vJpJLjHA4qmL\n"
                              "CDAd6CjAmE5ea4dFdZwDDS8F9FntJMdPQJA9vq+JaeS+k7ds3+7oiNe+RUIHR1Sz\n"
                              "VEAKh3Xw66kCgYB7KO/2Mchesu5qku2tZJhHF4QfP5cNcos511uO3bmJ3ln+16uR\n"
                              "GRqz7Vu0V6f7dvzPJM/O2QYqV5D9f9dHzN2YgvU9+QSlUeFK9PyxPv3vJt/WP1//\n"
                              "zf+nbpaRbwLxnCnNsKSQJFpnrE166/pSZfFbmZQpNlyeIuJU8czZGQTifQKBgHXe\n"
                              "/pQGEZhVNab+bHwdFTxXdDzr+1qyrodJYLaM7uFES9InVXQ6qSuJO+WosSi2QXlA\n"
                              "hlSfwwCwGnHXAPYFWSp5Owm34tbpp0mi8wHQ+UNgjhgsE2qwnTBUvgZ3zHpPORtD\n"
                              "23KZBkTmO40bIEyIJ1IZGdWO32q79nkEBTY+v/lRAoGBAI1rbouFYPBrTYQ9kcjt\n"
                              "1yfu4JF5MvO9JrHQ9tOwkqDmNCWx9xWXbgydsn/eFtuUMULWsG3lNjfst/Esb8ch\n"
                              "k5cZd6pdJZa4/vhEwrYYSuEjMCnRb0lUsm7TsHxQrUd6Fi/mUuFU/haC0o0chLq7\n"
                              "pVOUFq5mW8p0zbtfHbjkgxyF\n"
                              "-----END PRIVATE KEY-----\n";
}
#endif

namespace RTC
{

std::unique_ptr<Websocket> WebsocketTppFactory::Create() const
{
    return std::make_unique<WebsocketTpp>(GetUri(), CreateOptions());
}

#ifdef LOCAL_WEBSOCKET_TEST_SERVER

using namespace websocketpp;
using lib::placeholders::_1;
using lib::placeholders::_2;
using lib::bind;

class WebsocketTppTestFactory::TestServer
{
    using IncomingConnections = std::map<connection_hdl, volatile bool, std::owner_less<connection_hdl>>;
public:
    TestServer(const std::shared_ptr<MediaTimer>& timer, WebsocketTls tls);
    TestServer(const std::shared_ptr<MediaTimer>& timer, WebsocketOptions options);
    ~TestServer();
private:
    void Run();
    void OnMessage(connection_hdl hdl, MessagePtr message);
    void OnOpen(connection_hdl hdl);
    void OnClose(connection_hdl hdl);
    std::shared_ptr<asio::ssl::context> OnTlsInit(connection_hdl hdl);
    void BroadcastAudioWebm(const std::shared_ptr<MemoryBuffer>& buffer);
private:
    const std::shared_ptr<MediaTimer> _timer;
    WebsocketTls _tls;
    Server _server;
    std::thread _thread;
    ProtectedObj<IncomingConnections> _connections;
    uint64_t _timerId = 0U;
};

WebsocketTppTestFactory::WebsocketTppTestFactory(const std::shared_ptr<MediaTimer>& timer)
    : _testServer(std::make_unique<TestServer>(timer, CreateOptions()))
{
}

WebsocketTppTestFactory::WebsocketTppTestFactory()
    : WebsocketTppTestFactory(std::make_shared<MediaTimer>(GetUri() + " server"))
{
}

WebsocketTppTestFactory::~WebsocketTppTestFactory()
{
}

std::string WebsocketTppTestFactory::GetUri() const
{
    return _localUri + ":" + std::to_string(_port);
}


WebsocketTppTestFactory::TestServer::TestServer(const std::shared_ptr<MediaTimer>& timer,
                                                WebsocketTls tls)
    : _timer(timer)
    , _tls(std::move(tls))
{
    MS_ASSERT(_timer, "media timer must not be null");
    _tls._certificate = g_cert;
    _tls._certificatePrivateKey = g_certKey;
    _tls._certificateIsPem = true;
    _tls._peerVerification = WebsocketTlsPeerVerification::Yes;
    // Initialize ASIO
    _server.init_asio();
    // Register our handlers
    _server.set_message_handler(bind(&TestServer::OnMessage, this, _1, _2));
    _server.set_tls_init_handler(bind(&TestServer::OnTlsInit, this, _1));
    _server.set_open_handler(bind(&TestServer::OnOpen, this, _1));
    _server.set_close_handler(bind(&TestServer::OnClose, this, _1));
    // Listen on port 9002
    _thread = std::thread(std::bind(&TestServer::Run, this));
}

WebsocketTppTestFactory::TestServer::TestServer(const std::shared_ptr<MediaTimer>& timer,
                                                WebsocketOptions options)
    : TestServer(timer, std::move(options._tls))
{
}

WebsocketTppTestFactory::TestServer::~TestServer()
{
    {
        LOCK_WRITE_PROTECTED_OBJ(_connections);
        if (_timerId) {
            _timer->UnregisterTimer(_timerId);
            _timerId = 0;
        }
    }
    _server.stop();
    if (_thread.joinable()) {
        _thread.join();
    }
}

void WebsocketTppTestFactory::TestServer::Run()
{
    try {
        _server.listen(WebsocketTppTestFactory::_port);
        // Start the server accept loop
        _server.start_accept();
        // Start the ASIO io_service run loop
        _server.run();
    } catch (const std::exception& e) {
        MS_ERROR_STD("Exception: %s", e.what());
    }
}

void WebsocketTppTestFactory::TestServer::OnMessage(connection_hdl hdl, MessagePtr message)
{
    if (message) {
        LOCK_READ_PROTECTED_OBJ(_connections);
        const auto it = _connections->find(hdl);
        if (it != _connections->end()) {
            switch (message->get_opcode()) {
                case websocketpp::frame::opcode::text:
                    MS_DEBUG_DEV_STD("Received text '%s'", message->get_payload().c_str());
                    it->second = true;
                    break;
                case websocketpp::frame::opcode::binary:
                    MS_DEBUG_DEV_STD("Received binary %lu bytes", message->get_payload().size());
                    break;
                default:
                    break;
            }
        }
    }
}

void WebsocketTppTestFactory::TestServer::OnOpen(connection_hdl hdl)
{
    LOCK_WRITE_PROTECTED_OBJ(_connections);
    _connections->insert(std::make_pair(hdl, false));
    if (1U == _connections->size()) {
        _timerId = _timer->RegisterTimer([this]() {
            //static inline const char* _mockTranslationFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm";
            //static inline constexpr uint32_t _mockTranslationConnectionTimeoutMs = 1000U; // 1 sec
            BroadcastAudioWebm(FileReader::ReadAllAsBuffer("/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm"));
        });
        _timer->SetTimeout(_timerId, 4000 + 1000);
        _timer->Start(_timerId, false);
    }
}

void WebsocketTppTestFactory::TestServer::OnClose(connection_hdl hdl)
{
    LOCK_WRITE_PROTECTED_OBJ(_connections);
    _connections->erase(hdl);
    if (_connections->empty()) {
        _timer->UnregisterTimer(_timerId);
        _timerId = 0U;
    }
}

std::shared_ptr<asio::ssl::context> WebsocketTppTestFactory::TestServer::
    OnTlsInit(websocketpp::connection_hdl hdl)
{
    return WebsocketTppUtils::CreateSSLContext(_tls);
}

void WebsocketTppTestFactory::TestServer::BroadcastAudioWebm(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer) {
        LOCK_READ_PROTECTED_OBJ(_connections);
        for (auto it = _connections->begin(); it != _connections->end(); ++it) {
            if (it->second) {
                lib::error_code ec;
                _server.send(it->first, buffer->GetData(), buffer->GetSize(),
                             websocketpp::frame::opcode::binary, ec);
                if (ec) {
                    MS_ERROR_STD("broadcast audio failed: %s", ec.message().c_str());
                }
            }
        }
    }
}

#endif

} // namespace RTC
