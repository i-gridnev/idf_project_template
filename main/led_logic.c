#include <config_entry.h>
#include <device_config.h>
#include <esp_log.h>
#include <eventbus.h>

#include <digital_pin.h>
#include <led_logic.h>

#define TAG "LED_LOG"

// esp_err_t
// led_logic_handler(module_base* self, event_t* event) {
//     esp_err_t err = ESP_OK;
//     if (event->id == EVT_DIGITAL_PIN_STATE_CLICK) {
//         uint16_t clicks = event->payload.data.i32;
//         if (clicks == 1) {
//             led_state_t state = ws_ledstrip_get_state(0, CONFIG_LED_1);
//             led_cmd_t cmd = {.strip_id = 0, .led_id = CONFIG_LED_1};
//             if (state.opt.status == LED_ON) {
//                 cmd.type = LED_CMD_OFF;
//                 cmd.target_state._raw_value = 0;
//                 err = ws_ledstrip_send_cmd(&cmd);
//             } else if (state.opt.status == LED_OFF) {
//                 cmd.type = LED_CMD_SET_RGB;
//                 cmd.target_state.opt.status = LED_ON;
//                 cmd.target_state.opt.red = 0;
//                 cmd.target_state.opt.green = 255;
//                 cmd.target_state.opt.blue = 0;
//                 err = ws_ledstrip_send_cmd(&cmd);
//             }
//         } else if (clicks == 2) {
//             led_state_t state = ws_ledstrip_get_state(0, CONFIG_LED_0);
//             led_cmd_t cmd = {.strip_id = 0, .led_id = CONFIG_LED_0};
//             if (state.opt.status == LED_ON) {
//                 cmd.type = LED_CMD_OFF;
//                 cmd.target_state._raw_value = 0;
//                 err = ws_ledstrip_send_cmd(&cmd);
//             } else if (state.opt.status == LED_OFF) {
//                 cmd.type = LED_CMD_SET_RGB;
//                 cmd.target_state.opt.status = LED_ON;
//                 cmd.target_state.opt.red = 255;
//                 cmd.target_state.opt.green = 0;
//                 cmd.target_state.opt.blue = 0;
//                 err = ws_ledstrip_send_cmd(&cmd);
//             }
//         } else if (clicks == 3) {
//             led_cmd_t init_cmd = {.strip_id = 0, .type = LED_CMD_CLEAR_ALL};
//             err = ws_ledstrip_send_cmd(&init_cmd);
//         }
//     }
//     return err;
// }

// esp_err_t
// led_logic() {
//     ws_ledstrip_manager_config_t led_manager_cfg = {.strips_amount = 1, .event_handler = led_logic_handler};
//     module_base* led_manager = ws_ledstrip_manager_create(MODULE_LED_MANAGER, &led_manager_cfg);

//     ws_ledstrip_config_t led_strip_cfg = {.gpio = 1, .leds_amount = 5, .invert_out = false, .with_dma = false};
//     esp_err_t err = ws_ledstrip_add_strip(0, &led_strip_cfg);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "failed to add strip err=%d(%s)", err, esp_err_to_name(err));
//         return err;
//     }

//     led_cmd_t init_cmd = {.strip_id = 0, .type = LED_CMD_CLEAR_ALL};
//     ws_ledstrip_send_cmd(&init_cmd);

//     return eventbus_module_subscribe(led_manager, MODULE_DI_BTN, EVT_DIGITAL_PIN_STATE_CLICK);
// }