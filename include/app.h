#pragma once
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "app_context.h"
#include "app_types.h"


#define ADC_CH  ADC_CHANNEL_6

class App{
public:
    bool start();
    bool stop();

    TaskHandle_t getButtonHandle() const { return ctx_.buttonHandle; }

private:

    static void producer_trampoline(void *pv);
    static void consumer_trampoline(void *pv);
    static void logger_trampoline(void *pv);
    static void health_trampoline(void *pv);
    static void button_trampoline(void* pv);
    static void ui_trampoline(void* pv);
    static void uart_trampoline(void* pv);

    void producer();
    void consumer();
    void logger();
    void health();
    void button();
    void ui_task();
    void uart();

    void inc_dropped_logs();
    uint32_t get_dropped_logs();

    void handle_status();
    void handle_toggle_period(uint32_t ms);
    void handle_toggle_pause();

    AppContext ctx_{};

    static constexpr int POOL_N = 8;
    Sample pool_[POOL_N];
};