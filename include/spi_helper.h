#pragma once
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

struct Spl06Cal {
    int32_t c0, c1;
    int32_t c00, c10;
    int32_t c01, c11, c20, c21, c30;
};

extern Spl06Cal spl_cal;

static spi_device_handle_t spl06_dev;

esp_err_t spl06_spi_init();

esp_err_t spl06_read_reg(uint8_t reg, uint8_t *out);

esp_err_t spl06_read_burst(uint8_t start_reg, uint8_t *out, size_t n);

esp_err_t spl06_write_reg(uint8_t reg, uint8_t val);

void spl06_parse_calib(const uint8_t b[18], Spl06Cal *c);

void spl06_compensate(int32_t p_raw, int32_t t_raw,
                             uint8_t prs_cfg, uint8_t tmp_cfg,
                             float *temp_c, float *press_pa);