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

typedef struct{
    module_base module;
    int t_int1;
    int t_int2;
    char* name;
} t_issuer_m;

esp_err_t t_issuer_handler(module_base* self, event_t* event){

}

t_issuer_m*
t_issuer_create(int id, char* name){
    t_issuer_m* m = malloc(sizeof(t_issuer_m));

    module_base_config_t config ={
        .id = id,
        .max_evts = 5,
        .event_handler = t_issuer_handler,
    };
    module_create(&m->module, &config);
}

void
app_main(void) {
    ESP_LOGI(TAG, "...starting...");
    /*======= Component initialization block =======*/
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, ":::Initialized RAM: free=%lu, min=%lu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());


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