#include <esp_log.h>
#include <esp_timer.h>

#include <string.h>

#include "ws_ledstrip.h"

#define TAG                          "LED"

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ         (10 * 1000 * 1000)
#define LED_STRIP_MEMORY_BLOCK_WORDS 1024 // this determines the DMA block size if DMA is avaliable

#define LED_MANAGER_TASK_STACK       4096
#define LED_MANAGER_TASK_PRIO        5
#define LED_MANAGER_TASK_CORE        1
#define LED_MANAGER_TICK_MS          20
#define LED_MANAGER_QUEUE_TIMEOUT_MS 50
#define LED_MANAGER_QUEUE_SIZE       50

#define LOCK_MAX_MS                  100

DEVICE_MODULE_REGISTER(LEDSTRIP_MODULE);

struct ws_strip {
    int id;
    bool need_update;
    led_strip_handle_t handle;
    SLIST_ENTRY(ws_strip) next;
};

SLIST_HEAD(ws_strip_head, ws_strip);

typedef struct {
    struct ws_strip_head* stripes;
    TaskHandle_t task;
} _module_aux_t;

static _module_aux_t LEDSTRIP_MODULE_AUX;

// static bool
// _valid_strip_and_led(int strip_id, int led_id) {
//     if (strip_id >= MANAGER.config.strips_amount) {
//         ESP_LOGE(TAG, "invalid strip id %d", strip_id);
//         return false;
//     }
//     if (led_id >= MANAGER.stripes[strip_id].config.leds_amount) {
//         ESP_LOGE(TAG, "invalid led id %d in strip id %d", led_id, strip_id);
//         return false;
//     }
//     return true;
// }

// esp_err_t
// ws_ledstrip_send_cmd(led_cmd_t* cmd) {
//     if (!_valid_strip_and_led(cmd->strip_id, cmd->led_id)) {
//         return ESP_FAIL;
//     }

//     esp_err_t err = ESP_OK;
//     BaseType_t ret = xQueueSend(MANAGER.queue, cmd, pdMS_TO_TICKS(LED_MANAGER_QUEUE_TIMEOUT_MS));
//     if (ret != pdTRUE) {
//         ESP_LOGE(TAG, "manager queue is full, led cmd skipped!");
//         err = ESP_FAIL;
//     }
//     return err;
// }

// bool
// _update_active_state(ws_ledstrip_t* strip, int led_id) {
//     ws_led_t* led = &strip->leds[led_id];
//     bool need_update = false;
//     led->tick_counter++;
//     if (led->state.opt.status == LED_BLINK) {

//     } else if (led->state.opt.status == LED_FADE) {
//     }
//     return need_update;
// }

// bool
// _update_to_new_state(ws_ledstrip_t* strip, int led_id) {
//     ws_led_t* led = &strip->leds[led_id];
//     bool need_update = false;
//     if (led->state.opt.status != led->target_state.opt.status) {
//         need_update = true;
//         switch (led->target_state.opt.status) {
//             case LED_OFF:
//             case LED_ON:
//             case LED_BLINK:
//                 esp_err_t err = led_strip_set_pixel(strip->handle, led_id, led->target_state.opt.red,
//                                                     led->target_state.opt.green, led->target_state.opt.blue);
//                 if (err != ESP_OK) {
//                     ESP_LOGE(TAG, "Failed to upd led id=%d, err=%d(%s)", led_id, err, esp_err_to_name(err));
//                 } else {
//                     led->state._raw_value = led->target_state._raw_value;
//                 }
//                 break;
//         }
//     }
//     return need_update;
// }

// void
// _update_on_tick() {
//     for (int i = 0; i < MANAGER.config.strips_amount; i++) {
//         ws_ledstrip_t* strip = &MANAGER.stripes[i];
//         if (!strip) {
//             continue;
//         }
//         bool need_update = false;
//         for (int y = 0; y < strip->config.leds_amount; y++) {
//             need_update |= _update_active_state(strip, y);
//             need_update |= _update_to_new_state(strip, y);
//         }
//         if (need_update) {
//             ESP_LOGW(TAG, "upd");
//             led_strip_refresh(strip->handle);
//         }
//     }
// }

static esp_err_t
_tick_led(ws_led_t* led) {
    esp_err_t err = ESP_OK;
    led->tick_counter++;

    if (led->state == LED_STATE_ON) {
        err = led_strip_set_pixel(led->strip->handle, led->base.id, led->status.opt.red, led->status.opt.green,
                                  led->status.opt.blue);
        led->status.opt._on = true;
        led->strip->need_update = true;
    } else if (led->state == LED_STATE_OFF) {
        err = led_strip_set_pixel(led->strip->handle, led->base.id, 0, 0, 0);
        led->status.opt._on = false;
        led->strip->need_update = true;
    } else if (led->state == LED_STATE_BLINK) {
        if (led->status.opt._on && led->tick_counter == led->status_opt.blink.on_ms) {
            led->tick_counter = 0;
            err = led_strip_set_pixel(led->strip->handle, led->base.id, led->status.opt.red, led->status.opt.green,
                                      led->status.opt.blue);
            led->status.opt._on = true;
            led->strip->need_update = true;
        } else if (!led->status.opt._on && led->tick_counter == led->status_opt.blink.off_ms) {
            led->tick_counter = 0;
            err = led_strip_set_pixel(led->strip->handle, led->base.id, 0, 0, 0);
            led->status.opt._on = false;
            led->strip->need_update = true;
        }
    }
    return err;
}

static void
manager_task(void* arg) {
    esp_err_t err = ESP_OK;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(LED_MANAGER_TICK_MS * 1000));

        // grab mutex lock

        component_list_t* led_list_inst = NULL;
        SLIST_FOREACH(led_list_inst, LEDSTRIP_MODULE->components, next) {
            ws_led_t* led = (ws_led_t*)led_list_inst->component;
            err = _tick_led(led);
        }

        struct ws_strip* strip = NULL;
        SLIST_FOREACH(strip, LEDSTRIP_MODULE_AUX.stripes, next) {
            if (strip->need_update) {
                err = led_strip_refresh(strip->handle);
                strip->need_update = false;
            }
        }
    }
}

// while (true) {
//     xQueueReceive(MANAGER.queue, &led_cmd, portMAX_DELAY);
//     ws_ledstrip_t* strip = &MANAGER.stripes[led_cmd.strip_id];
//     if (strip) {
//         if (led_cmd.type == LED_CMD_TIMER_TICK) {
//             _update_on_tick();
//         } else if (led_cmd.type == LED_CMD_CLEAR_ALL) {
//             esp_err_t err = led_strip_clear(strip->handle);
//             if (err != ESP_OK) {
//                 ESP_LOGE(TAG, "strip_clear failed id=%d, err=%d(%s)", led_cmd.strip_id, err, esp_err_to_name(err));
//             } else {
//                 for (int i = 0; i < strip->config.leds_amount; i++) {
//                     ws_led_t* led = &strip->leds[i];
//                     memset(led, 0, sizeof(ws_led_t));
//                 }
//             }
//         } else {
//             ws_led_t* led = &strip->leds[led_cmd.led_id];
//             led->tick_counter = 0;
//             led->target_state._raw_value = led_cmd.target_state._raw_value;
//             switch (led_cmd.type) {
//                 case LED_CMD_SET_RGB: led->target_state.opt.status = LED_ON; break;
//                 case LED_CMD_BLINK:
//                     led->target_state.opt.status = LED_BLINK;
//                     memcpy(&led->blink, &led_cmd.cmd_opt.blink, sizeof(struct blink));
//                     break;
//                 case LED_CMD_FADE:
//                     led->target_state.opt.status = LED_FADE;
//                     memcpy(&led->fade, &led_cmd.cmd_opt.fade, sizeof(struct fade));
//                     break;
//                 case LED_CMD_OFF:
//                 default:
//                     led->target_state.opt.status = LED_OFF;
//                     memset(&led, 0, sizeof(ws_led_t));
//                     break;
//             }
//         }
//     }
//     memset(&led_cmd, 0, sizeof(led_cmd_t));
// }
// }

// static void
// _timer_callback(void* arg) {
//     led_cmd_t cmd = {.type = LED_CMD_TIMER_TICK};
//     BaseType_t ret = xQueueSend(MANAGER.queue, &cmd, pdMS_TO_TICKS(LED_MANAGER_QUEUE_TIMEOUT_MS));
//     if (ret != pdTRUE) {
//         ESP_LOGE(TAG, "manager queue is full, led cmd skipped!");
//     }
// }

// led_state_t
// ws_ledstrip_get_state(int strip_id, int led_id) {
//     led_state_t state = {0};
//     if (!_valid_strip_and_led(strip_id, led_id)) {
//         return state;
//     }
//     ws_ledstrip_t* strip = &MANAGER.stripes[strip_id];
//     ws_led_t* led = &strip->leds[led_id];
//     state._raw_value = led->state._raw_value;
//     return state;
// }

esp_err_t
ws_led_set(ws_led_t* led, led_state_t state, led_status_t status, led_status_opt_t* opt) {
    esp_err_t err = ESP_OK;
    led->tick_counter = 0;
    led->status = status;
    led->state = state;
    memcpy(&led->status_opt, opt, sizeof(led_status_opt_t));
    return err;
}

bool
filter_stripe_id_exist(ws_strip_t item, void* ctx) {
    int id = (int)ctx;
    return item->id == id;
}

ws_strip_t
ws_ledstrip_create(int id, int32_t gpio, uint32_t max_leds, bool with_dma, bool invert_out) {
    if (!LEDSTRIP_MODULE_AUX.task) { // On first call init all auxilary configs
        BaseType_t ret =
            xTaskCreatePinnedToCore(manager_task, "ledman", LED_MANAGER_TASK_STACK, NULL, LED_MANAGER_TASK_PRIO,
                                    &LEDSTRIP_MODULE_AUX.task, LED_MANAGER_TASK_CORE);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "led task failed with err=%d", ret);
            ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
        }
        // xSemaphoreCreateMutex()
    }

    struct ws_strip* stripe = NULL;
    if (SLIST_GET_WITH_TAIL(LEDSTRIP_MODULE_AUX.stripes, next, &stripe, filter_stripe_id_exist, (void*)id)) {
        ESP_LOGE(TAG, "stripe_id=%d already registered", id);
        return NULL;
    }
    struct ws_strip* new_stripe = calloc(1, sizeof(struct ws_strip));
    new_stripe->id = id;
    if (stripe) {
        SLIST_INSERT_AFTER(stripe, new_stripe, next);
    } else {
        SLIST_INSERT_HEAD(LEDSTRIP_MODULE_AUX.stripes, new_stripe, next);
    }
    stripe = new_stripe;

    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = max_leds,          // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812, // LED strip model
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = invert_out},
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = with_dma,
    };
    if (rmt_config.flags.with_dma) {
        rmt_config.mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS;
    } else {
        rmt_config.mem_block_symbols = 0;
    }
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &stripe->handle));
    return stripe;
}

ws_led_t*
ws_led_create(int id, ws_strip_t stripe, int index_position) {
    ws_led_t* led = calloc(1, sizeof(ws_led_t));
    led->strip = stripe;

    if (!device_module_add_component(index_position, &led->base, LEDSTRIP_MODULE)) {
        free(led);
        return NULL;
    }
    return led;
}
