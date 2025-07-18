#include <config_storage.h>
#include <eventbus_config.h>
#include "string.h"

config_entry_t DEVICE_CONFIG[CONFIG_MAX] = {
    [CONFIG_WIFI_SSID] =
        {
            .type = CFG_TYPE_STRING,
            .name = "wifi_ssid",
            .tag = "wf_ssid",
            .default_value.data.str = "",
            .default_value.size = strlen(""),
        },
    [CONFIG_WIFI_PASSWORD] =
        {
            .type = CFG_TYPE_STRING,
            .name = "wifi_password",
            .tag = "wf_pass",
            .default_value.data.str = "",
            .default_value.size = strlen(""),
        },
};

config_entry_t* CONFIG = DEVICE_CONFIG;
