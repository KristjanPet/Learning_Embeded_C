#pragma once
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static spi_device_handle_t spl06_dev;

esp_err_t spl06_spi_init();

esp_err_t spl06_read_reg(uint8_t reg, uint8_t *out);

esp_err_t spl06_read_burst(uint8_t start_reg, uint8_t *out, size_t n);

esp_err_t spl06_write_reg(uint8_t reg, uint8_t val);