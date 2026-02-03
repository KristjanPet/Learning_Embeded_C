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

esp_err_t sht31_read(float *temp_c, float *rh) {
    const uint8_t addr = 0x44;
    // Single shot, high repeatability, clock stretching disabled:
    const uint8_t cmd[2] = { 0x24, 0x00 };

    // Write command
    i2c_cmd_handle_t w = i2c_cmd_link_create();
    i2c_master_start(w);
    i2c_master_write_byte(w, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(w, cmd, sizeof(cmd), true);
    i2c_master_stop(w);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, w, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(w);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(20)); // measurement time

    // Read 6 bytes: T(msb,lsb,crc) RH(msb,lsb,crc)
    uint8_t data[6] = {0};

    i2c_cmd_handle_t r = i2c_cmd_link_create();
    i2c_master_start(r);
    i2c_master_write_byte(r, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(r, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(r);
    err = i2c_master_cmd_begin(I2C_NUM_0, r, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(r);
    if (err != ESP_OK) return err;

    // Convert raw values (ignore CRC for now)
    uint16_t rawT = (uint16_t(data[0]) << 8) | data[1];
    uint16_t rawH = (uint16_t(data[3]) << 8) | data[4];

    *temp_c = -45.0f + 175.0f * (float(rawT) / 65535.0f);
    *rh     = 100.0f * (float(rawH) / 65535.0f);

    return ESP_OK;
}
