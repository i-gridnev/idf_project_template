#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "wifi_network.h"

#define TAG                           "WIFI"

#define STATUS_STA_CONNECTION_ALLOWED BIT0
#define STATUS_STA_CONNECTED          BIT1
#define STATUS_STA_DISCONNECTED       BIT2
#define STATUS_STA_TRYING             BIT3

typedef struct {
    module_base module;
    int reconnect_interval_ms;
    int reconnect_attempts;

    int connect_attempts;
    EventGroupHandle_t status;
    esp_timer_handle_t reconnect_timer;
} wifi_network_module_obj;

static wifi_network_module_obj WIFI;

static void
_report(int id, bool success) {
    event_t evt = {0};
    evt.issuer = &WIFI.module;
    evt.id = id;
    evt.payload.data = (void*)success;
    eventbus_post_event(&evt);
}

static EventBits_t
_await_status(EventBits_t stat_bits, TickType_t ticks_timeout) {
    return xEventGroupWaitBits(WIFI.status, stat_bits, false, false, ticks_timeout);
}

static bool
_get_status(EventBits_t stat_bits) {
    return (xEventGroupGetBits(WIFI.status) & stat_bits);
}

static void
_reconnect_timer_callback(void* arg) {
    xEventGroupSetBits(WIFI.status, STATUS_STA_TRYING);
    _report(EVT_WIFI_STA_TRYING, true);
    esp_wifi_connect();
}

static void
_system_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START: ESP_LOGW(TAG, "STA_START"); break;
            case WIFI_EVENT_STA_STOP: ESP_LOGW(TAG, "STA_STOP"); break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "STA_DISCONNECTED");
                xEventGroupClearBits(WIFI.status, STATUS_STA_CONNECTED);
                xEventGroupSetBits(WIFI.status, STATUS_STA_DISCONNECTED);
                if (!is_wifi_network_STA_trying()) {
                    _report(EVT_WIFI_STA_CONNECTION, false);
                }

                if (WIFI.reconnect_attempts && _get_status(STATUS_STA_CONNECTION_ALLOWED)) {
                    if (WIFI.connect_attempts == WIFI.reconnect_attempts) {
                        ESP_LOGW(TAG, "Connect failed %d times, next try in %d ms", WIFI.reconnect_attempts,
                                 WIFI.reconnect_interval_ms);
                        WIFI.connect_attempts = 0;
                        xEventGroupClearBits(WIFI.status, STATUS_STA_TRYING);
                        _report(EVT_WIFI_STA_TRYING, false);
                        esp_timer_start_once(WIFI.reconnect_timer, WIFI.reconnect_interval_ms * 1000);
                    } else {
                        ESP_LOGW(TAG, "Trying to reconnect...");
                        if (!is_wifi_network_STA_trying()) {
                            xEventGroupSetBits(WIFI.status, STATUS_STA_TRYING);
                            _report(EVT_WIFI_STA_TRYING, true);
                        }
                        esp_wifi_connect();
                        WIFI.connect_attempts++;
                    }
                } else if (!_get_status(STATUS_STA_CONNECTION_ALLOWED)) {
                }
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGW(TAG, "STA_CONNECTED");
                WIFI.connect_attempts = 0;
                esp_timer_stop(WIFI.reconnect_timer);
                break;
            default: break;
        }

    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGW(TAG, "STA_GOT_IP");
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                ESP_LOGI(TAG, "My IP:" IPSTR, IP2STR(&event->ip_info.ip));

                xEventGroupClearBits(WIFI.status, STATUS_STA_TRYING);
                _report(EVT_WIFI_STA_TRYING, false);

                xEventGroupSetBits(WIFI.status, STATUS_STA_CONNECTED);
                xEventGroupClearBits(WIFI.status, STATUS_STA_DISCONNECTED);
                _report(EVT_WIFI_STA_CONNECTION, true);
                break;
            default: break;
        }
    }
}

esp_err_t
wifi_network_handler(module_base* self, event_t* event) {
    return ESP_OK;
}

esp_err_t
wifi_network_STA_connect(char* STA_ssid, char* STA_pass) {
    // if (_get_status(STATUS_STA_DISCONNECTED) && !_get_status(STATUS_STA_TRYING)) {

    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t wifi_sta_config = {0};
    esp_wifi_get_config(WIFI_IF_STA, &wifi_sta_config);
    strcpy((char*)wifi_sta_config.sta.ssid, STA_ssid);
    strcpy((char*)wifi_sta_config.sta.password, STA_pass);
    wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "STA set_config err=%d(%s)", err, esp_err_to_name(err));
        return err;
    }
    ESP_LOGW(TAG, "Mode STA, Connecting SSID:'%s' -> '%s'", (char*)wifi_sta_config.sta.ssid,
             (char*)wifi_sta_config.sta.password);

    WIFI.connect_attempts = 0;
    xEventGroupSetBits(WIFI.status, STATUS_STA_CONNECTION_ALLOWED);

    xEventGroupSetBits(WIFI.status, STATUS_STA_TRYING);
    _report(EVT_WIFI_STA_TRYING, true);
    return esp_wifi_connect();
    // }
    // return ESP_OK;
}

esp_err_t
wifi_network_STA_disconnect() {
    xEventGroupClearBits(WIFI.status, STATUS_STA_CONNECTION_ALLOWED);
    return esp_wifi_disconnect();
}

bool
is_wifi_network_STA_connected() {
    return _get_status(STATUS_STA_CONNECTED);
}

bool
is_wifi_network_STA_trying() {
    return _get_status(STATUS_STA_TRYING);
}

bool
wifi_network_await_STA_connect(int timeout_ms) {
    return (STATUS_STA_CONNECTED & _await_status(STATUS_STA_CONNECTED, timeout_ms * configTICK_RATE_HZ / 1000U));
}

bool
wifi_network_await_STA_disconnect(int timeout_ms) {
    return (STATUS_STA_DISCONNECTED & _await_status(STATUS_STA_DISCONNECTED, timeout_ms * configTICK_RATE_HZ / 1000U));
}

wifi_network_module*
wifi_network_create(int id, wifi_network_config_t* config) {
    WIFI.status = xEventGroupCreate();
    module_base_config_t base_config = {
        .id = id,
        .max_evts = EVT_WIFI_MAX,
        .event_handler = wifi_network_handler,
    };
    ESP_ERROR_CHECK(module_create(&WIFI.module, &base_config));

    ESP_ERROR_CHECK(esp_netif_init());
    if (config->mode == WIFI_MODE_STA) {
        esp_netif_create_default_wifi_sta();
    } else if (config->mode == WIFI_MODE_AP) {
        esp_netif_create_default_wifi_ap();
    } else if (config->mode == WIFI_MODE_APSTA) {
        esp_netif_create_default_wifi_sta();
        esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(config->mode);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_system_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_system_handler, NULL);

    WIFI.reconnect_attempts = config->reconnect_attempts;
    WIFI.reconnect_interval_ms = config->reconnect_interval_ms;
    const esp_timer_create_args_t timer_args = {.callback = &_reconnect_timer_callback};
    esp_timer_create(&timer_args, &WIFI.reconnect_timer);

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(eventbus_module_register(&WIFI.module));
    return (wifi_network_module*)&WIFI;
}