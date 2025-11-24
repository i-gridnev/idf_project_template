#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "wifi_network.h"

#define TAG                           "WIFI"

#define STATUS_STA_CONNECTION_ALLOWED BIT0
#define STATUS_STA_CONNECTED          BIT1
#define STATUS_STA_DISCONNECTED       BIT2
#define STATUS_STA_TRYING             BIT3

DEVICE_MODULE_REGISTER(WIFI_MODULE);

static void
_report(wifi_component_t* self, int id, bool success) {
    wifi_event_data_t data = {.success = success};
    event_t evt = {
        .issuer = &self->base,
        .id = id,
        .data = data.raw,
        .data_size = sizeof(wifi_event_data_t),
        .data_free_fcn = NULL,
    };
    device_post_event(&evt);
}

static EventBits_t
_await_status(EventBits_t stat_bits, TickType_t ticks_timeout) {
    wifi_component_t* self = (wifi_component_t*)device_module_get_component(WIFI_MODULE, SOLO_COMPONENT_ID);
    return xEventGroupWaitBits(self->status, stat_bits, false, false, ticks_timeout);
}

static bool
_get_status(wifi_component_t* self, EventBits_t stat_bits) {
    return (xEventGroupGetBits(self->status) & stat_bits);
}

static void
_reconnect_timer_callback(void* arg) {
    wifi_component_t* self = (wifi_component_t*)arg;
    xEventGroupSetBits(self->status, STATUS_STA_TRYING);
    _report(self, EVT_WIFI_STA_TRYING, true);
    esp_wifi_connect();
}

static void
_system_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    wifi_component_t* self = (wifi_component_t*)arg;
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START: _report(self, EVT_WIFI_READY, true); break;
            case WIFI_EVENT_STA_STOP: _report(self, EVT_WIFI_READY, false); break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "STA_DISCONNECTED");
                xEventGroupClearBits(self->status, STATUS_STA_CONNECTED);
                xEventGroupSetBits(self->status, STATUS_STA_DISCONNECTED);
                if (!is_wifi_network_STA_trying()) {
                    _report(self, EVT_WIFI_STA_CONNECTION, false);
                }

                if (self->config.reconnect_attempts && _get_status(self, STATUS_STA_CONNECTION_ALLOWED)) {
                    if (self->connect_attempts == self->config.reconnect_attempts) {
                        ESP_LOGW(TAG, "Connect failed %d times, next try in %d ms", self->config.reconnect_attempts,
                                 self->config.reconnect_interval_ms);
                        self->connect_attempts = 0;
                        xEventGroupClearBits(self->status, STATUS_STA_TRYING);
                        _report(self, EVT_WIFI_STA_TRYING, false);
                        esp_timer_start_once(self->reconnect_timer, self->config.reconnect_interval_ms * 1000);
                    } else {
                        ESP_LOGW(TAG, "Trying to reconnect...");
                        if (!is_wifi_network_STA_trying()) {
                            xEventGroupSetBits(self->status, STATUS_STA_TRYING);
                            _report(self, EVT_WIFI_STA_TRYING, true);
                        }
                        esp_wifi_connect();
                        self->connect_attempts++;
                    }
                } else if (!_get_status(self, STATUS_STA_CONNECTION_ALLOWED)) {
                }
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGW(TAG, "STA_CONNECTED");
                self->connect_attempts = 0;
                esp_timer_stop(self->reconnect_timer);
                break;
            default: break;
        }

    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGW(TAG, "STA_GOT_IP");
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                ESP_LOGI(TAG, "My IP:" IPSTR, IP2STR(&event->ip_info.ip));

                xEventGroupClearBits(self->status, STATUS_STA_TRYING);
                _report(self, EVT_WIFI_STA_TRYING, false);

                xEventGroupSetBits(self->status, STATUS_STA_CONNECTED);
                xEventGroupClearBits(self->status, STATUS_STA_DISCONNECTED);
                _report(self, EVT_WIFI_STA_CONNECTION, true);
                break;
            default: break;
        }
    }
}

esp_err_t
wifi_network_STA_connect(char* STA_ssid, char* STA_pass) {
    wifi_component_t* self = (wifi_component_t*)device_module_get_component(WIFI_MODULE, SOLO_COMPONENT_ID);
    wifi_network_STA_disconnect();
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

    self->connect_attempts = 0;
    xEventGroupSetBits(self->status, STATUS_STA_CONNECTION_ALLOWED);

    xEventGroupSetBits(self->status, STATUS_STA_TRYING);
    _report(self, EVT_WIFI_STA_TRYING, true);
    return esp_wifi_connect();
}

esp_err_t
wifi_network_STA_disconnect() {
    wifi_component_t* self = (wifi_component_t*)device_module_get_component(WIFI_MODULE, SOLO_COMPONENT_ID);
    xEventGroupClearBits(self->status, STATUS_STA_CONNECTION_ALLOWED);
    return esp_wifi_disconnect();
}

bool
is_wifi_network_STA_connected() {
    wifi_component_t* self = (wifi_component_t*)device_module_get_component(WIFI_MODULE, SOLO_COMPONENT_ID);
    return _get_status(self, STATUS_STA_CONNECTED);
}

bool
is_wifi_network_STA_trying() {
    wifi_component_t* self = (wifi_component_t*)device_module_get_component(WIFI_MODULE, SOLO_COMPONENT_ID);
    return _get_status(self, STATUS_STA_TRYING);
}

bool
wifi_network_await_STA_connect(int timeout_ms) {
    return (STATUS_STA_CONNECTED & _await_status(STATUS_STA_CONNECTED, timeout_ms * configTICK_RATE_HZ / 1000U));
}

bool
wifi_network_await_STA_disconnect(int timeout_ms) {
    return (STATUS_STA_DISCONNECTED & _await_status(STATUS_STA_DISCONNECTED, timeout_ms * configTICK_RATE_HZ / 1000U));
}

wifi_component_t*
wifi_network_create(wifi_component_config_t* config) {
    wifi_component_t* self = calloc(1, sizeof(wifi_component_t));
    memcpy(&self->config, config, sizeof(wifi_component_config_t));
    self->status = xEventGroupCreate();
    self->connect_attempts = 0;
    const esp_timer_create_args_t timer_args = {.callback = &_reconnect_timer_callback, .arg = self};
    esp_timer_create(&timer_args, &self->reconnect_timer);

    if (!device_module_add_component(SOLO_COMPONENT_ID, &self->base, WIFI_MODULE)) {
        free(self);
        return NULL;
    }

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

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_system_handler, self);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_system_handler, self);

    ESP_ERROR_CHECK(esp_wifi_start());
    return self;
}