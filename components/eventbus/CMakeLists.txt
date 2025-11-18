idf_component_register(SRCS "eventbus.c" "config_entry.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash esp_event)
