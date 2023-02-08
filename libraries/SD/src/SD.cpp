#include "SD.h"

#ifdef USE_TINYUSB
// For Serial when selecting TinyUSB.  Can't include in the core because Arduino IDE
// will not link in libraries called from the core.  Instead, add the header to all
// the standard libraries in the hope it will still catch some user cases where they
// use these libraries.
// See https://github.com/earlephilhower/arduino-pico/issues/167#issuecomment-848622174
#include <Adafruit_TinyUSB.h>
#endif

static_assert(__builtin_strcmp(SDClassFileMode(FILE_READ), "r") == 0, "");
static_assert(__builtin_strcmp(SDClassFileMode(FILE_WRITE), "a+") == 0, "");

static_assert(__builtin_strcmp(SDClassFileMode(O_RDONLY), "r") == 0, "");
static_assert(__builtin_strcmp(SDClassFileMode(O_WRONLY), "w+") == 0, "");
static_assert(__builtin_strcmp(SDClassFileMode(O_RDWR), "w+") == 0, "");
static_assert(__builtin_strcmp(SDClassFileMode(O_WRONLY | O_APPEND), "a") == 0, "");
static_assert(__builtin_strcmp(SDClassFileMode(O_RDWR | O_APPEND), "a+") == 0, "");

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SD)
SDClass SD;
#endif

void (*__SD__userDateTimeCB)(uint16_t*, uint16_t*) = nullptr;
