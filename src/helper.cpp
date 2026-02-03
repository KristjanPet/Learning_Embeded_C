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

esp_err_t ssd1306_cmd(uint8_t addr, const uint8_t *cmds, size_t n) {
    // control byte 0x00 = commands
    uint8_t buf[32];
    if (n + 1 > sizeof(buf)) return ESP_ERR_INVALID_SIZE;

    buf[0] = 0x00;
    for (size_t i = 0; i < n; i++) buf[i + 1] = cmds[i];

    i2c_cmd_handle_t c = i2c_cmd_link_create();
    i2c_master_start(c);
    i2c_master_write_byte(c, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(c, buf, n + 1, true);
    i2c_master_stop(c);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, c, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(c);
    return err;
}

esp_err_t ssd1306_data(uint8_t addr, const uint8_t *data, size_t n) {
    // control byte 0x40 = data
    i2c_cmd_handle_t c = i2c_cmd_link_create();
    i2c_master_start(c);
    i2c_master_write_byte(c, (addr << 1) | I2C_MASTER_WRITE, true);
    uint8_t control = 0x40;
    i2c_master_write(c, &control, 1, true);
    i2c_master_write(c, data, n, true);
    i2c_master_stop(c);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, c, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(c);
    return err;
}

esp_err_t ssd1306_init() {
    const uint8_t addr = 0x3C;
    const uint8_t init[] = {
        0xAE,       // display off
        0xD5, 0x80, // clock
        0xA8, 0x3F, // multiplex 1/64
        0xD3, 0x00, // offset
        0x40,       // start line
        0x8D, 0x14, // charge pump
        0x20, 0x00, // memory mode = horizontal
        0xA1,       // segment remap
        0xC8,       // COM scan dec
        0xDA, 0x12, // COM pins
        0x81, 0x7F, // contrast
        0xD9, 0xF1, // precharge
        0xDB, 0x40, // vcomh
        0xA4,       // resume RAM
        0xA6,       // normal display
        0xAF        // display on
    };
    return ssd1306_cmd(addr, init, sizeof(init));
}

esp_err_t ssd1306_clear() {
    const uint8_t addr = 0x3C;
    uint8_t set_addr[] = {
        0x21, 0x00, 0x7F, // col 0..127
        0x22, 0x00, 0x07  // page 0..7
    };
    ESP_ERROR_CHECK(ssd1306_cmd(addr, set_addr, sizeof(set_addr)));

    static uint8_t oled_zero[128]; // one page row
    memset(oled_zero, 0, sizeof(oled_zero));

    // 8 pages * 128 bytes
    for (int page = 0; page < 8; page++) {
        esp_err_t e = ssd1306_data(addr, oled_zero, sizeof(oled_zero));
        if (e != ESP_OK) return e;
    }
    return ESP_OK;
}
