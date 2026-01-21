#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "app_types.h"
#include "freertos/portmacro.h"   // for portMUX_TYPE
#include "esp_task_wdt.h"

struct AppContext{
    QueueHandle_t sampleQueue;
    QueueHandle_t logQueue;
    
    // For restart
    TaskHandle_t producerHandle;

    // Breadcrumb + heartbeat
    volatile int producer_stage;
    volatile uint32_t producer_heartbeat;

    // Dropped logs counter + lock
    portMUX_TYPE dropped_logs_mux;
    uint32_t dropped_logs;
};

bool app_init_and_start(AppContext *ctx);

static inline void inc_dropped_logs(AppContext *ctx);
static inline uint32_t get_dropped_logs(AppContext *ctx);

void health_task(void *pvParameters);
void logger_task(void *pvParameters);
void producer_task(void *pvParameters);
void consumer_task(void *pvParameters);