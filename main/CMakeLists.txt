idf_component_register(SRCS "logic/pin_logic.c" "logic/led_logic.c" "logic/web_logic.c" "main.c"
                    INCLUDE_DIRS "include"
                    EMBED_FILES ${CMAKE_SOURCE_DIR}/media/index.html
                    )