#pragma once // RTC/MediaTranslate/TranslatorDefines.hpp

#define MEDIA_TRANSLATIONS_TEST // TODO: remove it for production

#ifdef MEDIA_TRANSLATIONS_TEST

//#define NO_TRANSLATION_SERVICE
#define SINGLE_TRANSLATION_POINT_CONNECTION
//#define WRITE_TRANSLATION_TO_FILE
#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder
//#define READ_PRODUCER_RECV_FROM_FILE

#ifndef NO_TRANSLATION_SERVICE
//#define LOCAL_WEBSOCKET_TEST_SERVER
#endif

#endif // MEDIA_TRANSLATIONS_TEST