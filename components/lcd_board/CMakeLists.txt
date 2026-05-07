idf_component_register(SRCS "lcd_board.c"
                       INCLUDE_DIRS "."
                       REQUIRES tca9554 ili9341 driver)
