#pragma once // RTC/MediaTranslate/TranslatorDefines.hpp

#define MEDIA_TRANSLATIONS_TEST // TODO: remove it for production

#ifdef MEDIA_TRANSLATIONS_TEST
#define USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION

#define NO_TRANSLATION_SERVICE
#define SINGLE_TRANSLATION_POINT_CONNECTION
//#define WRITE_TRANSLATION_TO_FILE
//#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder

#ifndef NO_TRANSLATION_SERVICE
//#define LOCAL_WEBSOCKET_TEST_SERVER
#endif

#define MOCK_WEBM_INPUT_FILE "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_mono_example.webm"
// in seconds
#define MOCK_WEBM_INPUT_FILE_LEN_SECS 3U

#endif // MEDIA_TRANSLATIONS_TEST
