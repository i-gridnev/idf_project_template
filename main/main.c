#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#include <config_storage.h>
#include <device_config.h>
#include <eventbus.h>
#include <ws_ledstrip.h>

#include <led_logic.h>
#include <pin_logic.h>
#include <web_logic.h>

/* ==== heap memory watch (testing) ==== */
// #include "esp_heap_trace.h"
// #define NUM_RECORDS 100
// static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
/* =========================== */

#define TAG "APP"

void
app_main(void) {
    ESP_LOGI(TAG, "...starting...");
    /*======= Component initialization block =======*/
    ESP_LOGI(TAG, ":::Initialized RAM: free=%lu, min=%lu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

    ESP_ERROR_CHECK(cfg_init(CONFIG, CONFIG_MAX));

    ESP_ERROR_CHECK(eventbus_init(MODULES_MAX));

    ESP_ERROR_CHECK(web_logic());
    ESP_ERROR_CHECK(pin_logic());
    ESP_ERROR_CHECK(led_logic());

    /*==============================================*/

    // ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
    // ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
    // ....
    // ESP_ERROR_CHECK( heap_trace_stop() );
    // heap_trace_dump();
}