#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#include <device_config.h>
#include <eventbus.h>
#include <logic.h>

/* ==== heap memory watch (testing) ==== */
// #include "esp_heap_trace.h"
// #define NUM_RECORDS 100
// static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
/* =========================== */

#define TAG "APP"

config_entry_t CONFIG[CONFIG_MAX] = {
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
REGISTER_CONFIG(CONFIG, CONFIG_MAX)

void
app_main(void) {
    ESP_LOGI(TAG, "...starting...");
    ESP_LOGI(TAG, ":::Initialized RAM: free=%lu, min=%lu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    ESP_ERROR_CHECK(device_init());

    ESP_ERROR_CHECK(web_logic());
    ESP_ERROR_CHECK(pin_logic());
    // ESP_ERROR_CHECK(led_logic());

    /*==============================================*/

    // ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
    // ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
    // ....
    // ESP_ERROR_CHECK( heap_trace_stop() );
    // heap_trace_dump();
}