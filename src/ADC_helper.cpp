#include "ADC_helper.h"

adc_oneshot_unit_handle_t adc1_handle = nullptr;
adc_cali_handle_t cali_handle = nullptr;
bool cali_ok = false;

void adc_init(){
    // 1) Create ADC1 unit
    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc1_handle));

    // 2) Configure one channel
    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;       // usually 12-bit
    chan_cfg.atten = ADC_ATTEN_DB_11;               // up to ~3.3V range (approx)
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &chan_cfg));

}

static int to_percent(int raw){
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (raw * 100) / 4095;
}

void adc_task(){

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &raw));

    if (cali_ok) {
        int mv = 0;
        if (adc_cali_raw_to_voltage(cali_handle, raw, &mv) == ESP_OK) {
            ESP_LOGI(TAG_ADC, "raw=%d  mv=%d", raw, mv);
        } else {
            ESP_LOGI(TAG_ADC, "raw=%d  (mv conv failed)", raw);
        }
    } else {
        int percent = to_percent(raw);
        ESP_LOGI(TAG_ADC, "raw=%d, percent=%d%%", raw, percent);
    }
}
