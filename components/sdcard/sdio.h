#ifndef __SDIO_H__
#define __SDIO_H__

#include "stdio.h"
#include "string.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

//SD卡引脚定义
#define BSP_SD_CLK (15)
#define BSP_SD_CMD (7)
#define BSP_SD_D0  (4)

//文件行最大字符数
#define SDIO_FIFELINE_MAX_CHAR_SIZE (1024)

//SD卡操作句柄
extern sdmmc_card_t* sd_card_handle;

/**
 * @brief 初始化 SD 卡并挂载文件系统
 * 
 * @return void
 */
void sdcard_init(void);

/**
 * @brief 向 SD 卡写入文件
 * 
 * @param path 文件路径
 * @param data 待写入的数据
 * @return esp_err_t 错误代码
 */
esp_err_t sdio_write_file(const char* path, const char* data);

/**
 * @brief 从 SD 卡读取文件
 * 
 * @param path 文件路径
 * @return esp_err_t 错误代码
 */
esp_err_t sdio_read_file(const char* path);

/**
 * @brief SD 卡读写测试函数
 * 
 * @return void
 */
void sdcard_test(void);

/**
 * @brief 向 SD 卡写入二进制文件
 * 
 * @param path 文件路径
 * @param data 待写入的数据
 * @param len 数据长度
 * @return esp_err_t 错误代码
 */
esp_err_t sdio_write_binary_file(const char *path, const uint8_t *data, size_t len);

#endif /* __SDIO_H__ */