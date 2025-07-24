#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "config_storage.h"
#include "device_config.h"
#include "digital_pin.h"
#include "eventbus.h"
#include "web_ui.h"
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

    ESP_ERROR_CHECK(cfg_init(CONFIG, CONFIG_MAX));

    ESP_ERROR_CHECK(eventbus_init(MODULES_MAX));

    wifi_network_config_t wifi_con = {
        .mode = WIFI_MODE_STA,
        .reconnect_attempts = 3,
        .reconnect_interval_ms = 7000,
    };
    wifi_network_create(MODULE_WIFI_NET, &wifi_con);

    web_ui_create(MODULE_WEB_UI);

    wifi_network_STA_connect(CONFIG[CONFIG_WIFI_SSID].value.data.str, CONFIG[CONFIG_WIFI_PASSWORD].value.data.str);
    if (!wifi_network_await_STA_connect(5000)) {
        ESP_LOGI(TAG, "FAILLED");
        wifi_network_STA_disconnect();
    }

    digital_pin_config_t btn_cnf = {
        .gpio = 0,
        .active_level = 0,
        .disable_pull = false,
        .long_press_time = 2000,
        .short_press_time = 180,
        .init_state = false,
    };
    digital_pin_create(MODULE_DI_BTN, &btn_cnf);

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