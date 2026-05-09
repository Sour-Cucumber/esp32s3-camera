idf_component_register(SRCS "camera_board.c"
                       INCLUDE_DIRS "."
                       REQUIRES  driver esp32-camera lcd_board)
