#include "RTC/MediaTranslate/Websocket/WebsocketTppFactory.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTpp.hpp"
#ifdef LOCAL_WEBSOCKET_TEST_SERVER
#define MS_CLASS "RTC::WebsocketTppFactory::TestServer"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTppUtils.hpp"
#include "Logger.hpp"
#include "ProtectedObj.hpp"
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <atomic>
#include <map>
#include <thread>
#endif

#ifdef LOCAL_WEBSOCKET_TEST_SERVER
namespace {

using Server = websocketpp::server<websocketpp::config::asio_tls>;
using MessagePtr = websocketpp::config::asio::message_type::ptr;

class Client : public RTC::MediaTimerCallback
{
public:
    Client(const websocketpp::connection_hdl& hdl, std::string_view filename,
           std::weak_ptr<Server> serverRef);
    void SetReceivedTextMessage(const std::string& text);
    void AddReceivedBinarySize(size_t size) { _receivedBinarySize.fetch_add(size); }
    bool HasReceivedTextMessage() const { return _receivedTextMessage.load(); }
    bool HasEnoughReceivedBinary() const { return _receivedBinarySize.load() >= _translationThreshold; }
    bool IsReadyForSendTranslation() const;
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    static inline constexpr uint64_t _translationThreshold = 4U * 1024U; // 4kb
    const websocketpp::connection_hdl _hdl;
    const std::string_view _filename;
    const std::weak_ptr<Server> _serverRef;
    std::atomic_bool _receivedTextMessage;
    std::atomic<uint64_t> _receivedBinarySize = 0UL;
};

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

class WebsocketTppTestFactory::MockServer
{
    using IncomingConnection = std::pair<volatile uint64_t, std::shared_ptr<Client>>;
    using IncomingConnections = std::map<connection_hdl, IncomingConnection, std::owner_less<connection_hdl>>;
public:
    MockServer(const std::shared_ptr<MediaTimer>& timer, WebsocketTls tls);
    MockServer(const std::shared_ptr<MediaTimer>& timer, WebsocketOptions options);
    ~MockServer();
    bool IsValid() const { return _fileIsAccessible && _thread.joinable(); }
private:
    void Run();
    void OnMessage(connection_hdl hdl, MessagePtr message);
    void OnOpen(connection_hdl hdl);
    void OnClose(connection_hdl hdl);
    std::shared_ptr<asio::ssl::context> OnTlsInit(connection_hdl hdl);
private:
    const std::shared_ptr<MediaTimer> _timer;
    const bool _fileIsAccessible;
    const std::shared_ptr<Server> _server;
    WebsocketTls _tls;
    std::thread _thread;
    ProtectedObj<IncomingConnections> _connections;
};

WebsocketTppTestFactory::WebsocketTppTestFactory(const std::shared_ptr<MediaTimer>& timer)
    : _server(std::make_unique<MockServer>(timer, CreateOptions()))
{
}

WebsocketTppTestFactory::WebsocketTppTestFactory()
    : WebsocketTppTestFactory(std::make_shared<MediaTimer>(GetUri() + " server"))
{
}

WebsocketTppTestFactory::~WebsocketTppTestFactory()
{
}

bool WebsocketTppTestFactory::IsValid() const
{
    return _server->IsValid();
}

std::string WebsocketTppTestFactory::GetUri() const
{
    return _localUri + ":" + std::to_string(_port);
}

WebsocketTppTestFactory::MockServer::MockServer(const std::shared_ptr<MediaTimer>& timer,
                                                WebsocketTls tls)
    : _timer(timer)
    , _fileIsAccessible(FileReader::IsValidForRead(WebsocketTppTestFactory::_filename))
    , _server(std::make_shared<Server>())
    , _tls(std::move(tls))
{
    MS_ASSERT(_timer, "media timer must not be null");
    if (_fileIsAccessible) {
        _server->set_user_agent("WebsocketTppTestFactory");
        _tls._certificate = g_cert;
        _tls._certificatePrivateKey = g_certKey;
        _tls._certificateIsPem = true;
        _tls._peerVerification = WebsocketTlsPeerVerification::Yes;
        // Initialize ASIO
        _server->init_asio();
        // Register our handlers
        _server->set_message_handler(bind(&MockServer::OnMessage, this, _1, _2));
        _server->set_tls_init_handler(bind(&MockServer::OnTlsInit, this, _1));
        _server->set_open_handler(bind(&MockServer::OnOpen, this, _1));
        _server->set_close_handler(bind(&MockServer::OnClose, this, _1));
        // Listen on port
        _thread = std::thread(std::bind(&MockServer::Run, this));
        // wait
        while (!_thread.joinable()) {
            std::this_thread::yield();
        }
    }
    else {
        MS_ERROR_STD("unable to read %s", WebsocketTppTestFactory::_filename);
    }
}

WebsocketTppTestFactory::MockServer::MockServer(const std::shared_ptr<MediaTimer>& timer,
                                                WebsocketOptions options)
    : MockServer(timer, std::move(options._tls))
{
}

WebsocketTppTestFactory::MockServer::~MockServer()
{
    if (_fileIsAccessible) {
        _server->stop();
        if (_thread.joinable()) {
            _thread.join();
        }
        _server->set_message_handler(nullptr);
        _server->set_tls_init_handler(nullptr);
        _server->set_open_handler(nullptr);
        _server->set_close_handler(nullptr);
        LOCK_WRITE_PROTECTED_OBJ(_connections);
        for (auto it = _connections->begin(); it != _connections->end(); ++it) {
            _timer->Unregister(it->second.first);
        }
        _connections->clear();
    }
}

void WebsocketTppTestFactory::MockServer::Run()
{
    try {
        _server->listen(WebsocketTppTestFactory::_port);
        // Start the server accept loop
        _server->start_accept();
        // Start the ASIO io_service run loop
        _server->run();
    } catch (const std::exception& e) {
        MS_ERROR_STD("Exception: %s", e.what());
    }
}

void WebsocketTppTestFactory::MockServer::OnMessage(connection_hdl hdl, MessagePtr message)
{
    if (message && _fileIsAccessible) {
        LOCK_READ_PROTECTED_OBJ(_connections);
        const auto it = _connections->find(hdl);
        if (it != _connections->end()) {
            bool skip = false;
            switch (message->get_opcode()) {
                case websocketpp::frame::opcode::text:
                    it->second.second->SetReceivedTextMessage(message->get_payload());
                    break;
                case websocketpp::frame::opcode::binary:
                    it->second.second->AddReceivedBinarySize(message->get_payload().size());
                    break;
                default:
                    skip = true;
                    break;
            }
            if (!skip && !it->second.first && it->second.second->IsReadyForSendTranslation()) {
                it->second.first = _timer->RegisterAndStart(it->second.second, _repeatIntervalMs);
            }
        }
    }
}

void WebsocketTppTestFactory::MockServer::OnOpen(connection_hdl hdl)
{
    if (_fileIsAccessible) {
        LOCK_WRITE_PROTECTED_OBJ(_connections);
        if (!_connections->count(hdl)) {
            auto client = std::make_shared<Client>(hdl, WebsocketTppTestFactory::_filename, _server);
            _connections->insert(std::make_pair(hdl, std::make_pair(0U, std::move(client))));
        }
    }
}

void WebsocketTppTestFactory::MockServer::OnClose(connection_hdl hdl)
{
    LOCK_WRITE_PROTECTED_OBJ(_connections);
    const auto it = _connections->find(hdl);
    if (it != _connections->end()) {
        _timer->Unregister(it->second.first);
        _connections->erase(it);
    }
}

std::shared_ptr<asio::ssl::context> WebsocketTppTestFactory::MockServer::
    OnTlsInit(websocketpp::connection_hdl hdl)
{
    return WebsocketTppUtils::CreateSSLContext(_tls);
}

#endif

} // namespace RTC

#ifdef LOCAL_WEBSOCKET_TEST_SERVER
namespace {

Client::Client(const websocketpp::connection_hdl& hdl, std::string_view filename,
               std::weak_ptr<Server> serverRef)
    : _hdl(hdl)
    , _filename(std::move(filename))
    , _serverRef(std::move(serverRef))
{
}

void Client::SetReceivedTextMessage(const std::string& text)
{
    if (!_receivedTextMessage.exchange(true)) {
        MS_DEBUG_DEV_STD("Received text '%s'", text.c_str());
    }
}

bool Client::IsReadyForSendTranslation() const
{
    return HasReceivedTextMessage() && HasEnoughReceivedBinary();
}

void Client::OnEvent()
{
    if (!_hdl.expired()) {
        if (const auto server = _serverRef.lock()) {
            const auto content = RTC::FileReader::ReadAllAsBinary(_filename);
            if (!content.empty()) {
                websocketpp::lib::error_code ec;
                server->send(_hdl, content.data(), content.size(),
                             websocketpp::frame::opcode::binary, ec);
                if (ec) {
                    MS_ERROR_STD("broadcast audio failed: %s", ec.message().c_str());
                }
            }
        }
    }
}

}

#endif
