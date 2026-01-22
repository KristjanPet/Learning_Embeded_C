#pragma once
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"   // for portMUX_TYPE
#include "esp_task_wdt.h"
#include "app_context.h"
#include "app_types.h"

class App{
public:
    bool start();

private:

    static void producer_trampoline(void *pv);
    // static void consumer_trampoline(void *pv);
    static void logger_trampoline(void *pv);
    // static void health_trampoline(void *pv);

    void producer();
    // void consumer();
    void logger();
    // void health();

    AppContext ctx_{};
};