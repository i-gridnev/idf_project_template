#include <esp_log.h>
#include <esp_timer.h>

#include <string.h>

#include "ws_ledstrip.h"

#define TAG                     "LED"

#define LED_STRIP_RMT_RES_HZ    (10 * 1000 * 1000) // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_MEM_BLK_WORDS 1024               // this determines the DMA block size if DMA is avaliable

#define LED_MANAGER_TASK_STACK  4096
#define LED_MANAGER_TASK_PRIO   5
#define LED_MANAGER_TASK_CORE   1
#define LED_MANAGER_TICK_MS     20
#define LED_MANAGER_LOCK_MAX_MS 100

DEVICE_MODULE_REGISTER(LEDSTRIP_MODULE);

struct ws_strip {
    int32_t gpio;
    bool need_update;
    led_strip_handle_t handle;
    SLIST_ENTRY(ws_strip) next;
};

SLIST_HEAD(ws_strip_head, ws_strip);

typedef struct {
    struct ws_strip_head* stripes;
    TaskHandle_t task;
    SemaphoreHandle_t lock;
} _module_aux_t;

static _module_aux_t LEDSTRIP_MODULE_AUX;

static esp_err_t
_tick_led(ws_led_t* led) {
    esp_err_t err = ESP_OK;
    led->tick_counter++;
    if (led->status == LED_STATE_ON) {
        if (led->status != led->prev_status) {
            err = led_strip_set_pixel(led->strip->handle, led->base.id, led->red, led->green, led->blue);
            led->_on = true;
            led->strip->need_update = true;
        }
    } else if (led->status == LED_STATE_OFF) {
        if (led->status != led->prev_status) {
            err = led_strip_set_pixel(led->strip->handle, led->base.id, 0, 0, 0);
            led->_on = false;
            led->strip->need_update = true;
        }
    } else if (led->status == LED_STATE_BLINK) {
        if (led->status != led->prev_status || (!led->_on && led->tick_counter == led->status_opt.blink.off_ms)) {
            led->tick_counter = 0;
            err = led_strip_set_pixel(led->strip->handle, led->base.id, led->red, led->green, led->blue);
            led->_on = true;
            led->strip->need_update = true;
        } else if (led->_on && led->tick_counter == led->status_opt.blink.on_ms) {
            led->tick_counter = 0;
            err = led_strip_set_pixel(led->strip->handle, led->base.id, 0, 0, 0);
            led->_on = false;
            led->strip->need_update = true;
        }
    } else if (led->status == LED_STATE_BLINK_REPEAT) {
        if (led->status != led->prev_status
            || (!led->_on && led->repeat_counter == led->status_opt.blink_repeat.repeat
                && led->tick_counter == led->status_opt.blink_repeat.repeat_delay_ms)) {
            led->repeat_counter = 0;
            led->tick_counter = 0;
            err = led_strip_set_pixel(led->strip->handle, led->base.id, led->red, led->green, led->blue);
            led->_on = true;
            led->strip->need_update = true;
        } else if (!led->_on && led->tick_counter == led->status_opt.blink_repeat.off_ms) {
            led->tick_counter = 0;
            err = led_strip_set_pixel(led->strip->handle, led->base.id, led->red, led->green, led->blue);
            led->_on = true;
            led->strip->need_update = true;
        } else if (led->_on && led->tick_counter == led->status_opt.blink_repeat.on_ms) {
            led->repeat_counter++;
            led->tick_counter = 0;
            err = led_strip_set_pixel(led->strip->handle, led->base.id, 0, 0, 0);
            led->_on = false;
            led->strip->need_update = true;
        }
    }
    led->status = led->prev_status;
    return err;
}

static void
manager_task(void* arg) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(LED_MANAGER_TICK_MS * 1000));

        if (xSemaphoreTake(LEDSTRIP_MODULE_AUX.lock, pdMS_TO_TICKS(LED_MANAGER_LOCK_MAX_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "task failed with lock");
            continue;
        }

        component_list_t* led_list_item = NULL;
        SLIST_FOREACH(led_list_item, LEDSTRIP_MODULE->components, next) {
            ws_led_t* led = (ws_led_t*)led_list_item->component;
            _tick_led(led);
        }

        struct ws_strip* strip = NULL;
        SLIST_FOREACH(strip, LEDSTRIP_MODULE_AUX.stripes, next) {
            if (strip->need_update) {
                esp_err_t err = led_strip_refresh(strip->handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "failed strip refresh on gpio=%d, err=%d(%s)", (int)strip->gpio, err,
                             esp_err_to_name(err));
                }
                strip->need_update = false;
            }
        }
        xSemaphoreGive(LEDSTRIP_MODULE_AUX.lock);
    }
}

// led_state_e
// ws_ledstrip_get_state(int strip_id, int led_id) {
//     led_state_e state = {0};
//     if (!_valid_strip_and_led(strip_id, led_id)) {
//         return state;
//     }
//     ws_ledstrip_t* strip = &MANAGER.stripes[strip_id];
//     ws_led_t* led = &strip->leds[led_id];
//     state._raw_value = led->state._raw_value;
//     return state;
// }

esp_err_t
ws_led_set(int led_id, led_status_e status, uint8_t r, uint8_t g, uint8_t b, led_status_opt_t* opt) {
    ws_led_t* led = (ws_led_t*)device_module_get_component(LEDSTRIP_MODULE, led_id);
    if (led == NULL) {
        ESP_LOGE(TAG, "led id=%d not found", led_id);
        return ESP_ERR_NOT_FOUND;
    }

    if (xSemaphoreTake(LEDSTRIP_MODULE_AUX.lock, pdMS_TO_TICKS(LED_MANAGER_LOCK_MAX_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "led id=%d remains locked", led_id);
        return ESP_ERR_INVALID_STATE;
    }

    led->tick_counter = 0;
    led->repeat_counter = 0;
    led->status = status;
    led->red = r;
    led->green = g;
    led->blue = b;
    if (opt) {
        memcpy(&led->status_opt, opt, sizeof(led_status_opt_t));
    } else {
        memset(&led->status_opt, 0, sizeof(led_status_opt_t));
    }

    xSemaphoreGive(LEDSTRIP_MODULE_AUX.lock);
    return ESP_OK;
}

bool
filter_stripe_exist(ws_strip_t item, void* ctx) {
    int32_t gpio = (int)ctx;
    return item->gpio == gpio;
}

ws_strip_t
ws_ledstrip_create(int32_t gpio, uint32_t max_leds, bool with_dma, bool invert_out) {
    if (!LEDSTRIP_MODULE_AUX.task) { // On first call init all auxilary configs
        BaseType_t ret =
            xTaskCreatePinnedToCore(manager_task, "ledman", LED_MANAGER_TASK_STACK, NULL, LED_MANAGER_TASK_PRIO,
                                    &LEDSTRIP_MODULE_AUX.task, LED_MANAGER_TASK_CORE);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "led task failed with err=%d", ret);
            ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
        }
        LEDSTRIP_MODULE_AUX.lock = xSemaphoreCreateMutex();
        if (LEDSTRIP_MODULE_AUX.lock == NULL) {
            ESP_LOGE(TAG, "led lock init failed");
            ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
        }
    }

    struct ws_strip* stripe = NULL;
    if (SLIST_GET_WITH_TAIL(LEDSTRIP_MODULE_AUX.stripes, next, &stripe, filter_stripe_exist, (void*)gpio)) {
        ESP_LOGE(TAG, "stripe on gpio=%d already registered", (int)gpio);
        return NULL;
    }
    struct ws_strip* new_stripe = calloc(1, sizeof(struct ws_strip));
    new_stripe->gpio = gpio;
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
        rmt_config.mem_block_symbols = LED_STRIP_MEM_BLK_WORDS;
    } else {
        rmt_config.mem_block_symbols = 0;
    }
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &stripe->handle));
    ESP_ERROR_CHECK(led_strip_clear(stripe->handle));
    return stripe;
}

ws_led_t*
ws_led_create(int id, ws_strip_t stripe, int pos_index) {
    ws_led_t* led = calloc(1, sizeof(ws_led_t));
    led->strip = stripe;
    led->pos_index = pos_index;

    if (!device_module_add_component(id, &led->base, LEDSTRIP_MODULE)) {
        free(led);
        return NULL;
    }
    return led;
}
