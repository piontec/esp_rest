// This file was written by Lukasz Piatkowski
// [piontec -at- the well known google mail]
// and is distributed under GPL v2.0 license
// repo: https://github.com/piontec/esp_rest

#ifndef COMMON_H
#define COMMON_H

#define SOFT_VERSION "0.1"
#define DEBUG 1

#define STATION_MODE	0x01
#define SOFTAP_MODE	0x02
#define STATIONAP_MODE	0x03
#define CFG_WIFI_MODE 0x02

#define debug_print(...) \
            do { if (DEBUG) os_printf("DBG: "); os_printf(__VA_ARGS__); } while (0)

#endif
