#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"

#include "config_storage.h"

#define TAG                     "CNF"
#define FLOAT_STORAGE_PRECISION 10000 // Meaning saved as (int32_t)( val * FLOAT_STORAGE_PRECISION)

//===============================================================================//
//================================= Private =====================================//
//===============================================================================//

static char* CONFIG_NAMESPACE = "config";

esp_err_t
_init_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs_flash_init() warning, proceed with erase");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    nvs_stats_t nvs_stats;
    err = nvs_get_stats(NULL, &nvs_stats);
    ESP_LOGI(TAG, "Storage FreeEntries = (%d), AllEntries = (%d)", nvs_stats.free_entries, nvs_stats.total_entries);
    return err;
}

esp_err_t
_open_nvs(const char* namespace_name, nvs_open_mode_t open_mode, nvs_handle_t* nvs_handle) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, open_mode, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed load namespace '%s' err=%d(%s)", namespace_name, err, esp_err_to_name(err));
    } else {
        *nvs_handle = handle;
    }
    return err;
}

esp_err_t
_commit_nvs(nvs_handle_t handle, config_entry_t* entry) {
    esp_err_t err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed commit for '%s': error=%d(%s)", entry->tag, err, esp_err_to_name(err));
    }
    return err;
}

void
_print_entry(config_entry_t* entry) {
    if (entry->type == CFG_TYPE_INT || entry->type == CFG_TYPE_UNT16) {
        ESP_LOGW(TAG, " == '%s' = '%d'", entry->name,
                 (entry->type == CFG_TYPE_INT) ? entry->value.data.i32 : entry->value.data.u16);
    } else if (entry->type == CFG_TYPE_BOOL) {
        ESP_LOGW(TAG, " == '%s' = '%s'", entry->name, entry->value.data.b ? "true" : "false");
    } else if (entry->type == CFG_TYPE_FLOAT) {
        ESP_LOGW(TAG, " == '%s' = '%.3f'", entry->name, entry->value.data.f32);
    } else if (entry->type == CFG_TYPE_STRING) {
        ESP_LOGW(TAG, " == '%s' = '%s'", entry->name, entry->value.data.str);
    } else if (entry->type == CFG_TYPE_BLOB) {
        ESP_LOGW(TAG, " == '%s' = '%d databytes'", entry->name, entry->value.size);
    }
}

esp_err_t
_save_entry(nvs_handle_t nvs_handle, config_entry_t* entry) {
    esp_err_t err;
    if (entry->type == CFG_TYPE_INT) {
        err = nvs_set_i32(nvs_handle, entry->tag, entry->value.data.i32);
    } else if (entry->type == CFG_TYPE_UNT16) {
        err = nvs_set_u16(nvs_handle, entry->tag, entry->value.data.u16);
    } else if (entry->type == CFG_TYPE_BOOL) {
        int8_t ref = entry->value.data.b ? 1 : 0;
        err = nvs_set_i8(nvs_handle, entry->tag, ref);
    } else if (entry->type == CFG_TYPE_FLOAT) {
        float raw_value = entry->value.data.f32 * FLOAT_STORAGE_PRECISION;
        err = nvs_set_i32(nvs_handle, entry->tag, (int32_t)raw_value);
    } else if (entry->type == CFG_TYPE_STRING) {
        err = nvs_set_str(nvs_handle, entry->tag, entry->value.data.str);
    } else if (entry->type == CFG_TYPE_BLOB) {
        err = nvs_set_blob(nvs_handle, entry->tag, entry->value.data.ptr, entry->value.size);
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed save '%s': error=%d(%s)", entry->tag, err, esp_err_to_name(err));
    }
    return err;
}

esp_err_t
_use_default(nvs_handle_t nvs_handle, config_entry_t* entry) {
    if (entry->type >= CFG_TYPE_BOOL && entry->type <= CFG_TYPE_UNT16) {
        entry->value = entry->default_value;
    } else if (entry->type == CFG_TYPE_STRING || entry->type == CFG_TYPE_BLOB) {
        if (entry->value.data.ptr) {
            free(entry->value.data.ptr);
        }
        entry->value.data.ptr = malloc(entry->default_value.size);
        memcpy(entry->value.data.ptr, entry->default_value.data.ptr, entry->default_value.size);
    }
    return _save_entry(nvs_handle, entry);
}

esp_err_t
_load_entry(nvs_handle_t nvs_handle, config_entry_t* entry) {
    esp_err_t err;
    if (entry->type == CFG_TYPE_INT) {
        err = nvs_get_i32(nvs_handle, entry->tag, (int32_t*)&entry->value.data.i32);
    } else if (entry->type == CFG_TYPE_UNT16) {
        err = nvs_get_u16(nvs_handle, entry->tag, &entry->value.data.u16);
    } else if (entry->type == CFG_TYPE_BOOL) {
        int8_t ref;
        err = nvs_get_i8(nvs_handle, entry->tag, &ref);
        if (err == ESP_OK) {
            entry->value.data.b = (bool)ref;
        }
    } else if (entry->type == CFG_TYPE_FLOAT) {
        int32_t raw_value;
        err = nvs_get_i32(nvs_handle, entry->tag, &raw_value);
        if (err == ESP_OK) {
            entry->value.data.f32 = (float)raw_value / FLOAT_STORAGE_PRECISION;
        }
    } else if (entry->type == CFG_TYPE_STRING || entry->type == CFG_TYPE_BLOB) {
        size_t required_size;
        err = (entry->type == CFG_TYPE_STRING) ? nvs_get_str(nvs_handle, entry->tag, NULL, &required_size)
                                               : nvs_get_blob(nvs_handle, entry->tag, NULL, &required_size);
        if (err == ESP_OK) {
            void* ptr_buf = malloc(required_size);
            err = (entry->type == CFG_TYPE_STRING) ? nvs_get_str(nvs_handle, entry->tag, ptr_buf, &required_size)
                                                   : nvs_get_blob(nvs_handle, entry->tag, ptr_buf, &required_size);
            if (err == ESP_OK) {
                if (entry->value.data.ptr) {
                    free(entry->value.data.ptr);
                }
                entry->value.data.ptr = ptr_buf;
                entry->value.size = required_size;
            }
        }
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = _use_default(nvs_handle, entry);
        if (err == ESP_OK) {
            err = _commit_nvs(nvs_handle, entry);
        } else {
            ESP_LOGE(TAG, "Failed set default '%s': error=%d(%s)", entry->tag, err, esp_err_to_name(err));
        }
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed load '%s': error=%d(%s)", entry->tag, err, esp_err_to_name(err));
    }
    return err;
}

//===============================================================================//
//================================= Public ======================================//
//===============================================================================//

esp_err_t
cfg_init(config_entry_t* config, size_t entry_num) {
    nvs_handle_t nvs_handle;
    esp_err_t err = ESP_OK;
    ESP_ERROR_CHECK(_init_nvs());
    ESP_ERROR_CHECK(_open_nvs(CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle));
    for (int i = 0; i < entry_num; i++) {
        if (config[i].type >= CFG_TYPE_BOOL && config[i].type <= CFG_TYPE_UNT16) {
            config[i].value.data.i32 = 0;
            if (config[i].type == CFG_TYPE_BOOL) {
                config[i].value.size = sizeof(bool);
            } else if (config[i].type == CFG_TYPE_UNT16) {
                config[i].value.size = sizeof(uint16_t);
            } else if (config[i].type == CFG_TYPE_FLOAT) {
                config[i].value.size = sizeof(float);
            } else if (config[i].type == CFG_TYPE_INT) {
                config[i].value.size = sizeof(int32_t);
            }
        } else if (config[i].type == CFG_TYPE_STRING || config[i].type == CFG_TYPE_BLOB) {
            config[i].value.data.ptr = NULL;
            config[i].value.size = 0;
        }

        err = _load_entry(nvs_handle, &config[i]);
        if (err == ESP_OK) {
            _print_entry(&config[i]);
        } else {
            break;
        }
    }
    nvs_close(nvs_handle);
    return err;
}

esp_err_t
cfg_save_entry(config_entry_t* entry) {
    nvs_handle_t nvs_handle;
    esp_err_t err = _open_nvs(CONFIG_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = _save_entry(nvs_handle, entry);
    if (err == ESP_OK) {
        err = _commit_nvs(nvs_handle, entry);
        if (err == ESP_OK) {
            _print_entry(entry);
        }
    }
    nvs_close(nvs_handle);
    return err;
}

esp_err_t
cfg_reset_entry_to_default(config_entry_t* entry) {
    nvs_handle_t nvs_handle;
    esp_err_t err = _open_nvs(CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = _use_default(nvs_handle, entry);
    if (err == ESP_OK) {
        err = _commit_nvs(nvs_handle, entry);
        if (err == ESP_OK) {
            _print_entry(entry);
        }
    }
    nvs_close(nvs_handle);
    return err;
}

esp_err_t
cfg_reset_all_to_default(config_entry_t* config, size_t entry_num) {
    nvs_handle_t nvs_handle;
    esp_err_t err = _open_nvs(CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    for (int i = 0; i < entry_num; i++) {
        err = _use_default(nvs_handle, &config[i]);
        if (err != ESP_OK) {
            break;
        }
    }

    if (err == ESP_OK) {
        err = _commit_nvs(nvs_handle, NULL);
        if (err == ESP_OK) {
            ESP_LOGW(TAG, "RESETED TO DEFAULTS");
            for (int i = 0; i < entry_num; i++) {
                _print_entry(&config[i]);
            }
        }
    }

    nvs_close(nvs_handle);
    return err;
}