#include "sdio.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <errno.h>

static const char* TAG = "SDIO";

// SD 卡操作句柄定义
sdmmc_card_t* sd_card_handle = NULL;

char fileline[10][SDIO_FIFELINE_MAX_CHAR_SIZE]; // 定义缓冲区

// void sdcard_init(void) {
//     esp_err_t ret;

//     // 配置 SD 卡引脚
//     sdmmc_host_t host = SDMMC_HOST_DEFAULT();
//     host.slot = SDMMC_HOST_SLOT_1; // 使用 SDMMC_HOST_SLOT_1

//     // 配置 SD 卡引脚
//     gpio_set_pull_mode(BSP_SD_CLK, GPIO_PULLUP_ONLY);
//     gpio_set_pull_mode(BSP_SD_CMD, GPIO_PULLUP_ONLY);
//     gpio_set_pull_mode(BSP_SD_D0, GPIO_PULLUP_ONLY);

//     // 初始化 SD 卡
//     ret = sdmmc_host_init();
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to initialize SD card host: %s", esp_err_to_name(ret));
//         return;
//     }

//     // 初始化 SD 卡槽
//     ret = sdmmc_host_init_slot(host.slot, &host);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to initialize SD card slot: %s", esp_err_to_name(ret));
//         return;
//     }

//     // 挂载文件系统
//     esp_vfs_fat_sdmmc_mount_config_t mount_config = {
//         .format_if_mount_failed = true,
//         .max_files = 5,
//         .allocation_unit_size = 16 * 1024
//     };

//     ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &mount_config, &sd_card_handle);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to mount SD card filesystem: %s", esp_err_to_name(ret));
//         return;
//     }

//     ESP_LOGI(TAG, "SD card initialized and mounted successfully");
// }

/**
 * @brief 初始化 SD 卡并挂载文件系统
 */
void sdcard_init(void) {
    esp_err_t ret;

    // 挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,         // 如果挂载失败是否格式化 SD 卡
        .max_files = 5,                         // 最大同时打开的文件数
        .allocation_unit_size = 16 * 1024       // 分配单元为 16k
    };

    const char mount_point[] = "/sdcard"; // 定义挂载点

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");

    // SDMMC 主机接口配置， 直接使用默认配置
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // SDMMC 插槽配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;              // 使用 1 位数据线
    slot_config.clk = BSP_SD_CLK;       // SD_CLK 引脚
    slot_config.cmd = BSP_SD_CMD;       // SD_CMD 引脚    
    slot_config.d0 = BSP_SD_D0;         // SD_D0 引脚       
    slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 内部上拉 
    
    ESP_LOGI(TAG, "Mounting SD card filesystem");

    // 挂载 SD 卡文件系统
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &sd_card_handle);

    if(ret != ESP_OK)  {
        if(ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s)", esp_err_to_name(ret));
        }
        return;
    }

    ESP_LOGI(TAG, "Filesystem mounted");

    // 打印 SD 卡信息
    sdmmc_card_print_info(stdout, sd_card_handle);
}

/**
 * @brief 向 SD 卡写入文件
 * 
 * @param path 文件路径
 * @param data 待写入的数据
 * @return esp_err_t 错误代码
 */
esp_err_t sdio_write_file(const char* path, const char* data) {
    ESP_LOGI(TAG, "Writing file: %s", path);

    FILE* f = fopen(path, "w"); // 以写入模式打开文件

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);  // 关闭文件

    ESP_LOGI(TAG, "File written: %s", path);

    return ESP_OK;
}

/**
 * @brief 从 SD 卡读取文件
 * 
 * @param path 文件路径
 * @return esp_err_t 错误代码
 */
esp_err_t sdio_read_file(const char* path) {
    
    char *pos = NULL; // 定义指针变量

    ESP_LOGI(TAG, "Reading file: %s", path);

    FILE* f = fopen(path, "r"); // 以读取模式打开文件

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        return ESP_FAIL;
    }

    int line_count = 0;
    while (line_count < 10 && fgets(fileline[line_count], sizeof(fileline[line_count]), f)) {
        line_count++;
    }
    fclose(f); // 关闭文件

    pos = strchr(fileline[0], '\n'); // 查找换行符
    if (pos != NULL) {
        *pos = '\0'; // 将换行符替换为字符串结束符
    }

    line_count = 0;
    while (line_count < 10)
    {
        ESP_LOGI(TAG, "File content: %s", fileline[line_count]);
        line_count++;
    }
    
    return ESP_OK;
}

/**
 * @brief SD 卡读写测试函数
 */
void sdcard_test(void) {
    esp_err_t ret;
    const char* tfile = "/sdcard/testdir/mmtest.txt"; // 定义测试文件路径
    const char* testdir = "/sdcard/testdir/text.txt"; // 定义测试目录路径
    char data[SDIO_FIFELINE_MAX_CHAR_SIZE]; // 定义数据缓冲区

    mkdir("/sdcard/testdir", 0777); // 创建测试目录

    // 准备测试数据
    snprintf(data, SDIO_FIFELINE_MAX_CHAR_SIZE, "%s %s!\n", "girl", "mmgOOd");

    // 写入测试数据到 SD 卡
    ret = sdio_write_file(tfile, data);
    if (ret != ESP_OK) {
        return;
    }

    // 从 SD 卡读取数据并打印
    if (sdio_read_file(testdir) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read file");
        return;
    }

    // 卸载 SD 卡
    esp_vfs_fat_sdcard_unmount("/sdcard", sd_card_handle);
    ESP_LOGI(TAG, "SD card unmounted"); 
}

static esp_err_t __attribute__((unused)) sdio_write_binary_file_legacy(const char *path, const uint8_t *data, size_t len){
    FILE *f = fopen(path, "wb"); // 以二进制写入模式打开文件
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }
    size_t written = fwrite(data, 1, len, f); // 写入数据
    fclose(f); // 关闭文件
    if (written != len) {
        ESP_LOGE(TAG, "Write incomplete: %d/%d bytes", written, len);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Binary file written: %s (%d bytes)", path, len);
    return ESP_OK;
}

esp_err_t sdio_write_binary_file(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s, errno=%d", path, errno);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    if (written != len || ferror(f)) {
        ESP_LOGE(TAG, "Write incomplete: %u/%u bytes, errno=%d",
                 (unsigned)written, (unsigned)len, errno);
        fclose(f);
        return ESP_FAIL;
    }

    if (fflush(f) != 0) {
        ESP_LOGE(TAG, "Failed to flush file: %s, errno=%d", path, errno);
        fclose(f);
        return ESP_FAIL;
    }

    if (fclose(f) != 0) {
        ESP_LOGE(TAG, "Failed to close file: %s, errno=%d", path, errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Binary file written: %s (%u bytes)", path, (unsigned)len);
    return ESP_OK;
}
