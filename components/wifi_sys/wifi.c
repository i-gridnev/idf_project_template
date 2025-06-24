#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "wifi_sys.h"

#define TAG                  "WIFI"

#define STATUS_STA_STARTED   BIT0
#define STATUS_STA_CONNECTED BIT1

typedef struct {
    module_base module;
    int reconnect_interval_ms;
    int reconnect_attempts;

    int connect_attempts;
    bool need_reconnect;

    EventGroupHandle_t status;
    esp_timer_handle_t reconnect_timer;
} wifi_sys_module_obj;

static wifi_sys_module_obj WIFI_SYS;

static void
wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case WIFI_EVENT_STA_STOP:
            ESP_LOGW(TAG, "STA stopped");
            WIFI_SYS.connect_attempts = 0;
            esp_timer_stop(WIFI_SYS.reconnect_timer);
            // device_bus_post(WIFI.self.id, EVT_WIFI_STA_OFF, (bus_data_t)0, NULL);
            break;
        case WIFI_EVENT_STA_START:
            ESP_LOGW(TAG, "STA_START");
            xEventGroupSetBits(WIFI_SYS.status, STATUS_STA_STARTED);
            // device_bus_post(WIFI.self.id, EVT_WIFI_STA_ON, (bus_data_t)0, NULL);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "STA_DISCONNECTED");
            xEventGroupClearBits(WIFI_SYS.status, STATUS_STA_CONNECTED);
            // device_bus_post(WIFI.self.id, EVT_WIFI_STA_FAILED, (bus_data_t)0, NULL);
            if (WIFI_SYS.need_reconnect) {
                if (WIFI_SYS.connect_attempts == WIFI_SYS.reconnect_attempts) {
                    ESP_LOGW(TAG, "Connect failed %d times, next try in %d ms", WIFI_SYS.reconnect_attempts,
                             WIFI_SYS.reconnect_interval_ms);
                    WIFI_SYS.connect_attempts = 0;
                    esp_timer_start_once(WIFI_SYS.reconnect_timer, WIFI_SYS.reconnect_interval_ms * 1000);
                } else {
                    ESP_LOGW(TAG, "Trying to reconnect...");
                    esp_wifi_connect();
                    WIFI_SYS.connect_attempts++;
                }
            }
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGW(TAG, "STA_CONNECTED");
            WIFI_SYS.connect_attempts = 0;
            esp_timer_stop(WIFI_SYS.reconnect_timer);
            break;
        default: break;
    }
}

static void
ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            xEventGroupSetBits(WIFI_SYS.status, STATUS_STA_CONNECTED);
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            ESP_LOGW(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
            // device_bus_post(WIFI_SYS.self.id, EVT_WIFI_STA_CONNECTED, (bus_data_t)0, NULL);
            break;
        default: break;
    }
}

static void
reconnect_timer_callback(void* arg) {
    esp_wifi_connect();
}

esp_err_t
wifi_sys_handler(module_base* self, event_t* event) {
    return ESP_OK;
}

esp_err_t
wifi_sys_start_STA(char* STA_ssid, char* STA_pass, bool need_reconnect) {
    wifi_config_t wifi_sta_config = {0};
    esp_wifi_get_config(WIFI_IF_STA, &wifi_sta_config);
    esp_wifi_set_mode(WIFI_MODE_STA);

    strcpy((char*)wifi_sta_config.sta.ssid, STA_ssid);
    strcpy((char*)wifi_sta_config.sta.password, STA_pass);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed STA set_config err=%d(%s)", err, esp_err_to_name(err));
        return err;
    }
    ESP_LOGW(TAG, "Mode STA, Connecting SSID:'%s' -> '%s'", (char*)wifi_sta_config.sta.ssid,
             (char*)wifi_sta_config.sta.password);
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed wifi_start err=%d(%s)", err, esp_err_to_name(err));
        return err;
    }
    xEventGroupWaitBits(WIFI_SYS.status, STATUS_STA_STARTED, false, false, portMAX_DELAY);
    WIFI_SYS.connect_attempts = 0;
    WIFI_SYS.need_reconnect = need_reconnect;
    return esp_wifi_connect();
}

wifi_sys_module*
wifi_sys_create(int id, wifi_sys_config_t* config) {
    module_base_config_t base_config = {
        .id = id,
        .max_evts = EVT_WIFI_MAX,
        .event_handler = wifi_sys_handler,
    };
    module_create(&WIFI_SYS.module, &base_config);

    WIFI_SYS.status = xEventGroupCreate();

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed netif_init err=%d(%s)", err, esp_err_to_name(err));
        return NULL;
    }
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed wifi_init err=%d(%s)", err, esp_err_to_name(err));
        return NULL;
    }

    WIFI_SYS.reconnect_attempts = config->reconnect_attempts;
    WIFI_SYS.reconnect_interval_ms = config->reconnect_interval_ms;
    const esp_timer_create_args_t timer_args = {.callback = &reconnect_timer_callback};
    esp_timer_create(&timer_args, &WIFI_SYS.reconnect_timer);

    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    return (wifi_sys_module*)&WIFI_SYS;
}