#include "ADC_helper.h"

adc_oneshot_unit_handle_t adc1_handle = nullptr;
adc_cali_handle_t cali_handle = nullptr;
bool cali_ok = false;

static void pwm_init(){
    ledc_timer_config_t timer = {};
    timer.speed_mode       = LEDC_HIGH_SPEED_MODE;
    timer.timer_num        = LEDC_TIMER_0;
    timer.duty_resolution  = LEDC_TIMER_13_BIT;
    timer.freq_hz          = 5000;
    timer.clk_cfg          = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {};
    ch.speed_mode     = LEDC_HIGH_SPEED_MODE;
    ch.channel        = LEDC_CHANNEL_0;
    ch.timer_sel      = LEDC_TIMER_0;
    ch.intr_type      = LEDC_INTR_DISABLE;
    ch.gpio_num       = GPIO_NUM_2;
    ch.duty           = 0;
    ch.hpoint         = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

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

    pwm_init();
}

static void pwm_set_percent(int percent){
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    uint32_t duty = (uint32_t)((percent * 8191) / 100);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));
}


static int adc_raw_to_percent(int raw)
{
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (raw * 100) / 4095;
}

void adc_task(){

    int prev = -1;
    while(true){
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &raw));

        int pct = adc_raw_to_percent(raw);

        // small deadband so logs don’t spam
        if (pct != prev) {
            pwm_set_percent(pct);
            ESP_LOGI("PWM", "raw=%d -> %d%%", raw, pct);
            ESP_LOGI("PWM", "test duty 0 then max");
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));
            vTaskDelay(pdMS_TO_TICKS(800));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 8191));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));
            vTaskDelay(pdMS_TO_TICKS(800));
            ESP_LOGI("PWM", "End test");
            prev = pct;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
