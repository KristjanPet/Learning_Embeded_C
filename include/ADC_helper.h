#include "freertos/FreeRTOS.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_err.h"

static const char* TAG_ADC = "ADC";

void adc_init();
void adc_task();