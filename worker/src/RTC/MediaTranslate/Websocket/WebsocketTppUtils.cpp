#define MS_CLASS "RTC::WebsocketTppUtils"
#include "RTC/MediaTranslate/Websocket/WebsocketTppUtils.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTls.hpp"
#include "Logger.hpp"

namespace {

inline asio::ssl::context::method Convert(RTC::WebsocketTlsMethod method) {
    switch (method) {
        case RTC::WebsocketTlsMethod::sslv2:
            return asio::ssl::context::method::sslv2;
        case RTC::WebsocketTlsMethod::sslv2_client:
            return asio::ssl::context::method::sslv2_client;
        case RTC::WebsocketTlsMethod::sslv2_server:
            return asio::ssl::context::method::sslv2_server;
        case RTC::WebsocketTlsMethod::sslv3:
            return asio::ssl::context::method::sslv3;
        case RTC::WebsocketTlsMethod::sslv3_client:
            return asio::ssl::context::method::sslv3_client;
        case RTC::WebsocketTlsMethod::sslv3_server:
            return asio::ssl::context::method::sslv3_server;
        case RTC::WebsocketTlsMethod::tlsv1:
            return asio::ssl::context::method::tlsv1;
        case RTC::WebsocketTlsMethod::tlsv1_client:
            return asio::ssl::context::method::tlsv1_client;
        case RTC::WebsocketTlsMethod::tlsv1_server:
            return asio::ssl::context::method::tlsv1_server;
        case RTC::WebsocketTlsMethod::sslv23:
            return asio::ssl::context::method::sslv23;
        case RTC::WebsocketTlsMethod::sslv23_client:
            return asio::ssl::context::method::sslv23_client;
        case RTC::WebsocketTlsMethod::sslv23_server:
            return asio::ssl::context::method::sslv23_server;
        case RTC::WebsocketTlsMethod::tlsv11:
            return asio::ssl::context::method::tlsv11;
        case RTC::WebsocketTlsMethod::tlsv11_client:
            return asio::ssl::context::method::tlsv11_client;
        case RTC::WebsocketTlsMethod::tlsv11_server:
            return asio::ssl::context::method::tlsv11_server;
        case RTC::WebsocketTlsMethod::tlsv12:
            return asio::ssl::context::method::tlsv12;
        case RTC::WebsocketTlsMethod::tlsv12_client:
            return asio::ssl::context::method::tlsv12_client;
        case RTC::WebsocketTlsMethod::tlsv12_server:
            return asio::ssl::context::method::tlsv12_server;
        case RTC::WebsocketTlsMethod::tlsv13:
            return asio::ssl::context::method::tlsv13;
        case RTC::WebsocketTlsMethod::tlsv13_client:
            return asio::ssl::context::method::tlsv13_client;
        case RTC::WebsocketTlsMethod::tlsv13_server:
            return asio::ssl::context::method::tlsv13_server;
        case RTC::WebsocketTlsMethod::tls:
            return asio::ssl::context::method::tls;
        case RTC::WebsocketTlsMethod::tls_client:
            return asio::ssl::context::method::tls_client;
        case RTC::WebsocketTlsMethod::tls_server:
            return asio::ssl::context::method::tls_server;
        default:
            MS_ASSERT(false, "unsupported SSL/TLS method");
            break;
    }
    return asio::ssl::context::method::sslv23;
}

}

namespace RTC
{

std::shared_ptr<asio::ssl::context> WebsocketTppUtils::CreateSSLContext(const WebsocketTls& tls)
{
    auto ctx = std::make_shared<asio::ssl::context>(Convert(tls._method));
    if (!tls._trustStore.empty()) {
        const auto& store = tls._trustStore;
        ctx->add_certificate_authority(asio::buffer(store.data(), store.size()));
    }
    if (!tls._certificate.empty()) {
        const auto& certificate = tls._certificate;
        const auto& key = tls._certificatePrivateKey;
        const auto format = tls._certificateIsPem ? asio::ssl::context::file_format::pem :
                                                    asio::ssl::context::file_format::asn1;
        ctx->use_certificate_chain(asio::buffer(certificate.data(), certificate.size()));
        ctx->use_private_key(asio::buffer(key.data(), key.size()), format);
        ctx->set_password_callback([password = tls._certificatePrivateKeyPassword](
            std::size_t /*size*/, asio::ssl::context_base::password_purpose /*purpose*/) {
                return password;
        });
    }
    if (!tls._sslCiphers.empty()) {
        const auto res = SSL_CTX_set_cipher_list(ctx->native_handle(), tls._sslCiphers.c_str());
        if (1 != res) {
            throw std::system_error(res, asio::error::get_ssl_category(), "SSL_CTX_set_cipher_list");
        }
    }
    if (!tls._dh.empty()) {
        const auto& dh = tls._dh;
        ctx->use_tmp_dh(asio::buffer(dh.data(), dh.size()));
    }
    if (!tls._trustStore.empty() || !tls._certificate.empty()) {
        const auto& verification = tls._peerVerification;
        if (WebsocketTlsPeerVerification::No != verification) {
            asio::ssl::context::verify_mode mode = asio::ssl::context::verify_peer;
            if (WebsocketTlsPeerVerification::YesAndRejectIfNoCert == verification) {
                mode |= asio::ssl::context::verify_fail_if_no_peer_cert;
            }
            ctx->set_verify_mode(mode);
        }
    }
    asio::ssl::context::options options = asio::ssl::context::default_workarounds;
    if (tls._dhSingle) {
        options |= asio::ssl::context::single_dh_use;
    }
    if (tls._sslv2No) {
        options |= asio::ssl::context::no_sslv2;
    }
    if (tls._sslv3No) {
        options |= asio::ssl::context::no_sslv3;
    }
    if (tls._tlsv1No) {
        options |= asio::ssl::context::no_tlsv1;
    }
    if (tls._tlsv1_1No) {
        options |= asio::ssl::context::no_tlsv1_1;
    }
    if (tls._tlsv1_2No) {
        options |= asio::ssl::context::no_tlsv1_2;
    }
    if (tls._tlsv1_3No) {
        options |= asio::ssl::context::no_tlsv1_3;
    }
    if (tls._sslNoCompression) {
        options |= asio::ssl::context::no_compression;
    }
    ctx->set_options(options);
    return ctx;
}

} // namespace RTC
