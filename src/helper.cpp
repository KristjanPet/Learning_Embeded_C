#include "helper.h"

esp_err_t i2c_master_init(){
    i2c_config_t conf{};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_21;
    conf.scl_io_num = GPIO_NUM_22;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    return i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

void i2c_scan(){
    ESP_LOGI("I2C", "Scanning....");
    int found = 0;

    for(int addr = 1; addr < 127; addr++){
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK) {
            ESP_LOGI("I2C", "Found device at 0x%02X", addr);
            found++;
        }
    }

    ESP_LOGI("I2C", "Scan done, found=%d", found);
}