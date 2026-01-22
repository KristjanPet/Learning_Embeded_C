#pragma once
#include <cstdint>
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"

struct Settings {
    uint32_t producer_period_ms;
};

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

    //Mutex
    SemaphoreHandle_t settingsMutex;
    Settings settings;
};