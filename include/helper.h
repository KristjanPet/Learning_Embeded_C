#pragma once
#include "driver/i2c.h"
#include "esp_log.h"

void i2c_scan();
esp_err_t i2c_master_init();
