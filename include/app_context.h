#pragma once
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h" // for portMUX_TYPE

struct Settings {
    uint32_t producer_period_ms;
};

struct AppContext{
    QueueHandle_t freeQ = nullptr;
    QueueHandle_t dataQ = nullptr;
    QueueHandle_t logQueue;
    
    // For restart
    TaskHandle_t loggerHandle;
    TaskHandle_t healthHandle;
    TaskHandle_t producerHandle;
    TaskHandle_t consumerHandle;

    // Stop flag
    volatile bool stopRequested = false;

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