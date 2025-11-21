idf_component_register(SRCS "wifi_network.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_timer esp_wifi eventbus_device)
