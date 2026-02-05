#pragma once
#include "driver/i2c.h"
#include "esp_log.h"

static uint8_t fb[128 * 8];

void i2c_scan();
esp_err_t i2c_master_init();
esp_err_t sht31_read(float *temp_c, float *rh);

esp_err_t ssd1306_cmd(uint8_t addr, const uint8_t *cmds, size_t n);
esp_err_t ssd1306_data(uint8_t addr, const uint8_t *data, size_t n);
esp_err_t ssd1306_init();
esp_err_t ssd1306_clear();
void draw_text(int x, int page, const char* s);
esp_err_t ssd1306_flush();