idf_component_register(SRCS "bus_module.c" "eventbus.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_event eventbus_config)
