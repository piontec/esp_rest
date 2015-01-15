/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 * minor cleanup by Lukasz Piatkowski
 * used in esp_rest project
 * repo: https://github.com/piontec/esp_rest
 */

#ifndef DHT22_h
#define DHT22_h

void ICACHE_FLASH_ATTR DHT(void);
float * ICACHE_FLASH_ATTR readDHT(void);
void DHTInit(void);

#endif
