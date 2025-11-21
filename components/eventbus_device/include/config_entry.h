#ifndef _CONFIG_STORAGE_H_
#define _CONFIG_STORAGE_H_

#include <stdbool.h>
#include <stdio.h>
#include "esp_err.h"

typedef enum {
    CFG_TYPE_BOOL,
    CFG_TYPE_FLOAT,
    CFG_TYPE_INT,
    CFG_TYPE_UNT16,
    CFG_TYPE_STRING,
    CFG_TYPE_BLOB,
} config_entry_type;

typedef struct {
    size_t size;

    union {
        void* ptr;
        char* str;
        int i32;
        uint16_t u16;
        float f32;
        bool b;
    } data;

} config_value_t;

typedef struct {
    config_entry_type type;
    char* name;
    char* tag;
    config_value_t value;
    config_value_t default_value;
} config_entry_t;

extern config_entry_t* DEVICE_CONFIG;
extern size_t DEVICE_CONFIG_SIZE;

#define REGISTER_CONFIG(config_array, config_size)                                                                     \
    config_entry_t* DEVICE_CONFIG = config_array;                                                                      \
    size_t DEVICE_CONFIG_SIZE = config_size;

esp_err_t _device_cfg_init();

esp_err_t device_cfg_save_entry(config_entry_t* entry);

esp_err_t device_cfg_entry_to_default(config_entry_t* entry);

esp_err_t device_cfg_all_to_default();

#endif /* _CONFIG_STORAGE_H_ */