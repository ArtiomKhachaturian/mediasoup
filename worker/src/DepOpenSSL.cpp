#define MS_CLASS "DepOpenSSL"
// #define MS_LOG_DEV_LEVEL 3

#include "DepOpenSSL.hpp"
#include "Logger.hpp"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <atomic>

/* Static. */

static std::atomic_bool initialized = false;

/* Static methods. */

void DepOpenSSL::ClassInit()
{
	MS_TRACE();
    if (!initialized.exchange(true)) {
        MS_DEBUG_TAG(info, "openssl version: \"%s\"", OpenSSL_version(OPENSSL_VERSION));
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        // Initialize some crypto stuff.
        RAND_poll();
    }
}


void DepOpenSSL::ClassDestroy()
{
    if (initialized.exchange(false)) {
        ERR_free_strings();
        EVP_cleanup();
        CRYPTO_cleanup_all_ex_data();
        CONF_modules_unload(1);
    }
}
