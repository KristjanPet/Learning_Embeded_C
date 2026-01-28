#pragma once
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "app_context.h"
#include "app_types.h"
#include "pool_queue.hpp"

class App{
public:
    bool start();
    bool stop();

private:

    static void producer_trampoline(void *pv);
    static void consumer_trampoline(void *pv);
    static void logger_trampoline(void *pv);
    static void health_trampoline(void *pv);

    void producer();
    void consumer();
    void logger();
    void health();

    void inc_dropped_logs();
    uint32_t get_dropped_logs();

    AppContext ctx_{};

    static constexpr int POOL_N = 8;
    Sample pool_[POOL_N];
    PoolQueue<Sample, POOL_N> samples_;
};