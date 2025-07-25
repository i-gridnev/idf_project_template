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

esp_err_t
t_handler(module_base* self, event_t* event) {
    if (event->id == EVT_DIGITAL_PIN_STATE_CLICK) {
        int num_clicks = event->payload.data.i32;
        module_base* o1 = eventbus_module_get(MODULE_DI_OU1);
        ESP_LOGI(TAG, "click %d", num_clicks);
        if (num_clicks == 1) {
            digital_pin_set_silent(o1, true);
        } else if (num_clicks == 2) {
            digital_pin_set_silent(o1, false);
        }
    }
    return ESP_OK;
}

esp_err_t
t2_handler(module_base* self, event_t* event) {
    module_base* o2 = eventbus_module_get(MODULE_DI_OU2);
    if (event->issuer->id == MODULE_DI_IN1) {
        bool state1 = event->payload.data.b;
        bool state2 = digital_pin_get(eventbus_module_get(MODULE_DI_IN2));
        digital_pin_set_silent(o2, state1 || state2);
    } else if (event->issuer->id == MODULE_DI_IN2) {
        bool state2 = event->payload.data.b;
        bool state1 = digital_pin_get(eventbus_module_get(MODULE_DI_IN1));
        digital_pin_set_silent(o2, state1 || state2);
    }
    return ESP_OK;
}

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

    // wifi_network_STA_connect(CONFIG[CONFIG_WIFI_SSID].value.data.str, CONFIG[CONFIG_WIFI_PASSWORD].value.data.str);
    // if (!wifi_network_await_STA_connect(5000)) {
    //     ESP_LOGI(TAG, "FAILLED");
    //     wifi_network_STA_disconnect();
    // }

    digital_pin_config_t btn_cnf = {
        .button =
            {.gpio = 0, .active_level = 0, .disable_pull = false, .long_press_time = 2000, .short_press_time = 180},
    };
    module_base* b1 = digital_pin_create(MODULE_DI_BTN, DIGITAL_PIN_TYPE_BUTTON, &btn_cnf);

    digital_pin_config_t in1_cnf = {.input = {.gpio = 4, .active_level = 0, .disable_pull = false}};
    module_base* i1 = digital_pin_create(MODULE_DI_IN1, DIGITAL_PIN_TYPE_INPUT, &in1_cnf);

    digital_pin_config_t in2_cnf = {.input = {.gpio = 5, .active_level = 0, .disable_pull = false}};
    module_base* i2 = digital_pin_create(MODULE_DI_IN2, DIGITAL_PIN_TYPE_INPUT_INVERSE, &in2_cnf);

    digital_pin_config_t out1_cnf = {.output = {.gpio = 1, .state = false}};
    module_base* o1 = digital_pin_create(MODULE_DI_OU1, DIGITAL_PIN_TYPE_OUTPUT, &out1_cnf);

    digital_pin_config_t out2_cnf = {.output = {.gpio = 2, .state = false}};
    module_base* o2 = digital_pin_create(MODULE_DI_OU2, DIGITAL_PIN_TYPE_OUTPUT, &out2_cnf);

    b1->event_handler = t_handler;
    module_subscribe(b1, b1->id, EVT_DIGITAL_PIN_STATE_CLICK);

    i2->event_handler = t2_handler;
    module_subscribe(i2, i2->id, EVT_DIGITAL_PIN_STATE_CLICK);
    module_subscribe(i2, i1->id, EVT_DIGITAL_PIN_STATE_CLICK);

    digital_pin_report_now(i1);
    digital_pin_report_now(i2);

    // vTaskDelay(pdMS_TO_TICKS(3000));
    // while (1) {
    //     digital_pin_set_silent(o1, digital_pin_get(i1));
    //     digital_pin_set_silent(o2, digital_pin_get(i2));

    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    /*==============================================*/

    // ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
    // ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
    // ....
    // ESP_ERROR_CHECK( heap_trace_stop() );
    // heap_trace_dump();
}