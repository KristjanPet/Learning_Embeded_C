#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "app_tasks.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void) {
    static AppContext ctx;
    ctx.dropped_logs_mux = portMUX_INITIALIZER_UNLOCKED;
    ctx.dropped_logs = 0;

    esp_task_wdt_config_t twdt_config =  {
        .timeout_ms = 5000, //5 sec
        .idle_core_mask = 0, //not watching idle tasks
        .trigger_panic = false //reset on timeout
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
    ctx.sampleQueue = xQueueCreate(5, sizeof(Sample)); //creates queue & and sets sizes
    if (ctx.sampleQueue == NULL){
        ESP_LOGE(TAG, "Failed to create sampleQueue");
        return;
    }
    ctx.logQueue = xQueueCreate(10, sizeof(LogEvent));
    if (ctx.logQueue == NULL){
        ESP_LOGE(TAG, "Failed to create logQueue");
        return;
    }
    
    start_app_tasks(&ctx);
}