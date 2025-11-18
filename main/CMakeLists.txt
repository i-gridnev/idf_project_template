idf_component_register(SRCS "pin_logic.c" "led_logic.c" "web_logic.c" "main.c"
                    INCLUDE_DIRS "include"
                    EMBED_FILES ${CMAKE_SOURCE_DIR}/media/index.html
                    )