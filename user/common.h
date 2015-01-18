// This file was written by Lukasz Piatkowski
// [piontec -at- the well known google mail]
// and is distributed under GPL v2.0 license
// repo: https://github.com/piontec/esp_rest

#ifndef COMMON_H
#define COMMON_H

#define DEBUG 1

#define STATION_MODE	0x01
#define SOFTAP_MODE	0x02
#define STATIONAP_MODE	0x03

#define debug_print(...) \
            do { if (DEBUG) os_printf("DBG: "); os_printf(__VA_ARGS__); } while (0)
//#define debug_print(fmt, ...) \
//            do { if (DEBUG) os_printf("DBG: %s", ##__VA_ARGS__); } while (0)
//#define debug_print(txt, ...) \
//            if (DEBUG) os_printf("DBG: %s", txt);
//#ifdef DEBUG
//    #define debug_print(txt) os_printf("DBG: %s", txt);
//#else
//    #define debug_print(txt)
//#endif

#endif
