idf_component_register(SRCS 
                        "hx711_multi.c" 
                        "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_timer)