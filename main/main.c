#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"

#include "eventbus.h"

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
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, ":::Initialized RAM: free=%lu, min=%lu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

    ModuleBase m1 = {.id = MODULE_1};
    ModuleBase m2 = {.id = MODULE_2};
    ModuleBase m3 = {.id = MODULE_3};
    ModuleBase m4 = {.id = MODULE_4};
    ModuleBase m5 = {.id = MODULE_5};

    eventbus_init();

    eventbus_register(&m1, GROUP_1);
    eventbus_register(&m1, GROUP_2);
    eventbus_register(&m1, GROUP_3);

    eventbus_register(&m2, GROUP_1);
    eventbus_register(&m2, GROUP_2);

    eventbus_register(&m3, GROUP_3);
    eventbus_register(&m3, GROUP_4);

    eventbus_register(&m5, GROUP_1);
    eventbus_register(&m5, GROUP_2);
    eventbus_register(&m5, GROUP_3);
    eventbus_register(&m5, GROUP_4);

    eventbus_print_layput();




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