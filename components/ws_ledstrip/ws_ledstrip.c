#include <esp_log.h>
#include <esp_timer.h>
#include <led_strip.h>
#include <string.h>

#include "ws_ledstrip.h"

#define TAG                          "LED"

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ         (10 * 1000 * 1000)
#define LED_STRIP_MEMORY_BLOCK_WORDS 1024 // this determines the DMA block size if DMA is avaliable

#define LED_BLINK_TICK_FRAME_MS      200
#define LOCK_MAX_MS                  500

typedef struct {
    led_state_t state;

    struct {
        bool need_blink;
        bool is_on;
        int tick_counter;
        int on_ticks;
        int off_ticks;
    } blink;
} ws_led_t;

typedef struct {
    module_base module;
    ws_ledstrip_config_t config;
    led_strip_handle_t handle;
    ws_led_t* led_registry;
    esp_timer_handle_t blink_timer;
    SemaphoreHandle_t lock;
} ws_ledstrip_t;

// static esp_err_t
// _set_led(led_strip_handle_t handle, int id, uint32_t red, uint32_t green, uint32_t blue) {
//     esp_err_t err = led_strip_set_pixel(handle, id, red, green, blue);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to set led id=%d, err=%d(%s)", id, err, esp_err_to_name(err));
//         return err;
//     }
//     err = led_strip_refresh(handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to refresh strip err=%d(%s)", err, esp_err_to_name(err));
//     }
//     return err;
// }

esp_err_t
ws_ledstrip_reset_all(module_base* self) {
    ws_ledstrip_t* strip = (ws_ledstrip_t*)self;
    esp_err_t err = ESP_FAIL;
    if (xSemaphoreTake(strip->lock, pdMS_TO_TICKS(LOCK_MAX_MS)) == pdTRUE) {
        for (int i = 0; i < strip->config.leds_amount; i++) {
            ws_led_t* led = &strip->led_registry[i];
            led->blink.need_blink = false;
            led->blink.tick_counter = 0;
            led->blink.is_on = 0;
            led->state.opt.red = 0;
            led->state.opt.green = 0;
            led->state.opt.blue = 0;
        }
        err = led_strip_clear(strip->handle);
        xSemaphoreGive(strip->lock);
    }
    return err;
}

esp_err_t
ws_ledstrip_set_led(module_base* self, led_state_t state) {
    ws_ledstrip_t* strip = (ws_ledstrip_t*)self;
    esp_err_t err = ESP_FAIL;

    if (xSemaphoreTake(strip->lock, pdMS_TO_TICKS(LOCK_MAX_MS)) == pdTRUE) {
        ws_led_t* led = &strip->led_registry[state.opt.index];
        led->blink.need_blink = false;
        led->blink.tick_counter = 0;
        led->state = state;

        err = led_strip_set_pixel(strip->handle, state.opt.index, state.opt.red, state.opt.green, state.opt.blue);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set led id=%d, err=%d(%s)", state.opt.index, err, esp_err_to_name(err));
            return err;
        }
        err = led_strip_refresh(strip->handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to refresh strip err=%d(%s)", err, esp_err_to_name(err));
        }
        xSemaphoreGive(strip->lock);
    } else {
        ESP_LOGE(TAG, "Failed led set, too busy");
    }

    return err;
}

esp_err_t
ws_ledstrip_set_led_blink(module_base* self, led_state_t state, int on_ms, int off_ms, bool start_with) {
    ws_ledstrip_t* strip = (ws_ledstrip_t*)self;
    esp_err_t err = ESP_FAIL;
    if (xSemaphoreTake(strip->lock, pdMS_TO_TICKS(LOCK_MAX_MS)) == pdTRUE) {
        ws_led_t* led = &strip->led_registry[state.opt.index];

        led->state = state;
        led->blink.need_blink = true;
        led->blink.tick_counter = 0;
        led->blink.on_ticks = on_ms / LED_BLINK_TICK_FRAME_MS;
        led->blink.off_ticks = off_ms / LED_BLINK_TICK_FRAME_MS;

        if (start_with) {
            led->blink.is_on = true;
            err = led_strip_set_pixel(strip->handle, state.opt.index, state.opt.red, state.opt.green, state.opt.blue);
        } else {
            led->blink.is_on = false;
            err = led_strip_set_pixel(strip->handle, state.opt.index, 0, 0, 0);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set led id=%d, err=%d(%s)", state.opt.index, err, esp_err_to_name(err));
        }
        err = led_strip_refresh(strip->handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to refresh strip err=%d(%s)", err, esp_err_to_name(err));
        }

        xSemaphoreGive(strip->lock);
    } else {
        ESP_LOGE(TAG, "Failed led set, too busy");
    }
    return err;
}

static void
_strip_timer_callback(void* arg) {
    ws_ledstrip_t* strip = (ws_ledstrip_t*)arg;
    if (xSemaphoreTake(strip->lock, pdMS_TO_TICKS(LOCK_MAX_MS)) == pdTRUE) {
        bool need_refresh = false;
        for (int i = 0; i < strip->config.leds_amount; i++) {
            ws_led_t* led = &strip->led_registry[i];
            if (led->blink.need_blink) {
                led->blink.tick_counter++;
                if (led->blink.is_on) {
                    if (led->blink.tick_counter >= led->blink.on_ticks) {
                        led->blink.is_on = false;
                        led->blink.tick_counter = 0;
                        led_strip_set_pixel(strip->handle, led->state.opt.index, 0, 0, 0);
                        need_refresh = true;
                    }
                } else {
                    if (led->blink.tick_counter >= led->blink.off_ticks) {
                        led->blink.is_on = true;
                        led->blink.tick_counter = 0;
                        led_strip_set_pixel(strip->handle, led->state.opt.index, led->state.opt.red,
                                            led->state.opt.green, led->state.opt.blue);
                        need_refresh = true;
                    }
                }
            }
        }
        if (need_refresh) {
            led_strip_refresh(strip->handle);
        }
        xSemaphoreGive(strip->lock);
    }
}

module_base*
ws_ledstrip_create(int id, ws_ledstrip_config_t* config) {
    ws_ledstrip_t* strip = calloc(1, sizeof(ws_ledstrip_t));
    memcpy(&strip->config, config, sizeof(ws_ledstrip_config_t));

    module_base_config_t base_config = {.id = id, .max_evts = 1, .event_handler = config->event_handler};
    ESP_ERROR_CHECK(eventbus_module_register(&strip->module, &base_config));

    led_strip_config_t strip_config = {
        .strip_gpio_num = config->gpio,
        .max_leds = config->leds_amount, // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,   // LED strip model
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = config->invert_out},
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = config->with_dma,
    };
    if (rmt_config.flags.with_dma) {
        rmt_config.mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS;
    } else {
        rmt_config.mem_block_symbols = 0;
    }
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip->handle));

    strip->led_registry = calloc(config->leds_amount, sizeof(ws_led_t));
    for (int i = 0; i < config->leds_amount; i++) {
        strip->led_registry[i].state.opt.index = i;
    }

    strip->lock = xSemaphoreCreateMutex();
    const esp_timer_create_args_t timer_args = {.callback = &_strip_timer_callback, .arg = strip};
    esp_err_t err = esp_timer_create(&timer_args, &strip->blink_timer);
    err |= esp_timer_start_periodic(strip->blink_timer, LED_BLINK_TICK_FRAME_MS * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed blink_timer err=%d(%s)", err, esp_err_to_name(err));
        return NULL;
    }
    return &strip->module;
}