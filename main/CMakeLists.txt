idf_component_register(SRCS "device_config.c" "web_ui.c" "main.c"
                    INCLUDE_DIRS "include"
                    EMBED_FILES ${CMAKE_SOURCE_DIR}/media/index.html
                    )