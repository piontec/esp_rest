/* Copyright (c) 2014 Ivan Grokhotkov. All rights reserved. 
 * This file is part of the atproto AT protocol library
 *
 * Redistribution and use is permitted according to the conditions of the
 * 3-clause BSD license to be found in the LICENSE.BSD file.
 */

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#define CONFIG_MAGIC   0x42 // the answer to the question of live and everything
#define CONFIG_VERSION 1
#define DEFAULT_INTERVAL 60
#define DEFAULT_BOOT_CFGMODE 1

#include <c_types.h>

typedef struct {
    uint32 magic;
    uint32 version;
    uint32 interval_sec;
    char boot_config;
    char uri [256];
    // bump CONFIG_VERSION when adding new fields
} config_t;


config_t* config_get();
void config_save();
config_t* config_init();
void config_init_default();

#endif//CONFIG_STORE_H
