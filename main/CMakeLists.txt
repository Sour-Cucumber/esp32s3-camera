idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES tca9554 lcd_board camera_board esp32-camera)
