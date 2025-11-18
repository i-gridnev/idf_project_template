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

extern config_entry_t* CONFIG;

esp_err_t _cfg_init(config_entry_t* config, size_t entry_num);

esp_err_t device_cfg_save_entry(config_entry_t* entry);

esp_err_t device_cfg_entry_to_default(config_entry_t* entry);

esp_err_t device_cfg_all_to_default(config_entry_t* config, size_t entry_num);

#endif /* _CONFIG_STORAGE_H_ */