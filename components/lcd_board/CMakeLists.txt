idf_component_register(SRCS "lcd_board.c"
                       INCLUDE_DIRS "."
                       REQUIRES tca9554 driver esp_lcd)
