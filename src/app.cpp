#include "app.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "APP";

bool App::start(){
    ctx_.dropped_logs_mux = portMUX_INITIALIZER_UNLOCKED;
    ctx_.dropped_logs = 0;
    ctx_.producer_stage = 0;
    ctx_.producer_heartbeat = 0;

    ctx_.sampleQueue = xQueueCreate(5, sizeof(Sample));

    if (xTaskCreate(&App::producer_trampoline, "producer", 2048, this, 3, &ctx_.producerHandle) != pdPASS){
        ESP_LOGE(TAG, "Failed to create producer task");
        return false;
    }

    return true;
}

void App::producer_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->producer();
}

void App::producer(){
    while (true){
        ctx_.producer_heartbeat++;
        ESP_LOGI(TAG, "Producer heartbeat: %d", ctx_.producer_heartbeat);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}