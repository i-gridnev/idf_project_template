#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "eventbus.h"
#include "wifi_network.h"

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
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(eventbus_init(MODULES_MAX));

    wifi_network_config_t wifi_con = {
        .mode = WIFI_MODE_STA,
        .reconnect_attempts = 3,
        .reconnect_interval_ms = 7000,
    };
    wifi_network_create(MODULE_WIFI_NET, &wifi_con);

    // while (1) {
    //     vTaskDelay(pdMS_TO_TICKS(2000));
    //     ESP_LOGI(TAG, "beep");
    // }
    /*==============================================*/

    // ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
    // ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
    // ....
    // ESP_ERROR_CHECK( heap_trace_stop() );
    // heap_trace_dump();
}