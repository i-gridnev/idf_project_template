idf_component_register(SRCS "webserver.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_https_server esp_timer eventbus_device)
