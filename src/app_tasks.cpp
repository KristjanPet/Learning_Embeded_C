#include "app_tasks.h"

bool app_init_and_start(AppContext *ctx){

    ctx->producerHandle = nullptr;
    ctx->producer_stage = 0;
    ctx->producer_heartbeat = 0;
    ctx->dropped_logs_mux = portMUX_INITIALIZER_UNLOCKED;
    ctx->dropped_logs = 0;

    ctx->sampleQueue = xQueueCreate(5, sizeof(Sample)); //creates queue & and sets sizes
    if (ctx->sampleQueue == NULL){
        ESP_LOGE("INIT", "Failed to create sampleQueue");
        return false;
    }
    ctx->logQueue = xQueueCreate(10, sizeof(LogEvent));
    if (ctx->logQueue == NULL){
        ESP_LOGE("INIT", "Failed to create logQueue");
        return false;
    }

    ctx->settingsMutex = xSemaphoreCreateMutex();
    if (ctx->settingsMutex == NULL){
        ESP_LOGE("INIT", "Failed to create settings Mutex");
        return false;
    }
    ctx->settings.producer_period_ms = 200;

    xTaskCreate(health_task, "health", 2048, ctx, 2, NULL);
    if (xTaskCreate(logger_task, "logger", 2048, ctx, 5, NULL) == pdFAIL){ ESP_LOGE("INIT", "logger task creation failed"); return false; }
    xTaskCreate(producer_task, "producer", 2048, ctx, 3, &ctx->producerHandle); //runs the sending task
    xTaskCreate(consumer_task, "consumer", 2048, ctx, 6, NULL); //runs the sending task
    return true;
}

static inline void inc_dropped_logs(AppContext *ctx) {
    portENTER_CRITICAL(&ctx->dropped_logs_mux);
    ctx->dropped_logs++;
    portEXIT_CRITICAL(&ctx->dropped_logs_mux);
}

static inline uint32_t get_dropped_logs(AppContext *ctx) {
  portENTER_CRITICAL(&ctx->dropped_logs_mux);
  uint32_t v = ctx->dropped_logs;
  portEXIT_CRITICAL(&ctx->dropped_logs_mux);
  return v;
}

void health_task(void *pvParameters){
    auto *ctx = (AppContext*)pvParameters;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    int prev_hb = 0;
    uint8_t stuck_seconds = 0;

    int toggleSendPeriod = 0;

    while(1){
        uint32_t v = get_dropped_logs(ctx);
        if (ctx->producer_heartbeat == prev_hb){ //check if stuck
            stuck_seconds++;
        }
        else {
            stuck_seconds = 0;
        }
        prev_hb = ctx->producer_heartbeat;
        if (stuck_seconds >= 6) //reboot if stuck
        {
            ESP_LOGE("HEALTH", "Producer task stuck, rebooting task now");
            if(ctx->producerHandle){
                esp_task_wdt_delete(ctx->producerHandle);
                vTaskDelete(ctx->producerHandle);
                ctx->producerHandle = nullptr;
            }
            xTaskCreate(producer_task, "producer", 2048, NULL, 3, &ctx->producerHandle);
            stuck_seconds = 0;
        }
        
        ESP_LOGI("HEALTH", "dropped_logs= %u, stage=%d", v, ctx->producer_stage);
        ESP_ERROR_CHECK(esp_task_wdt_reset());

        //toggle send period for mutex
        if (toggleSendPeriod >= 5){
            xSemaphoreTake(ctx->settingsMutex, portMAX_DELAY);
            ctx->settings.producer_period_ms = (ctx->settings.producer_period_ms == 200) ? 1000 : 200;
            xSemaphoreGive(ctx->settingsMutex);
            toggleSendPeriod = 0;
        }
        toggleSendPeriod++;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void logger_task(void *pvParameters){
    auto *ctx = (AppContext*)pvParameters;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task
    LogEvent ev;
    int n = 0;
    while(1){
        if(xQueueReceive(ctx->logQueue, &ev, portMAX_DELAY) != pdTRUE){
            ESP_LOGE("LOG", "Error receiving log");
        }else{
            switch (ev.type)
            {
                case LogType::SENT:
                    ESP_LOGI("LOG", "SENT count= %d, t= %u", ev.count, ev.timestamp_ms);
                    break;
                case LogType::RECEIVED:
                    ESP_LOGI("LOG", "RECEIVED count= %d, t= %u", ev.count, ev.timestamp_ms);
                    break;
                case LogType::DROPPED:
                    ESP_LOGW("LOG", "DROPPED count= %d, t= %u", ev.count, ev.timestamp_ms);
                    break;
                case LogType::ERROR:
                    ESP_LOGE("LOG", "ERROR while sending sample");
                default:
                    break;
            }  
        }
        n++;
        if(n % 10 == 0){
            ESP_LOGI("LOG", "producer stage: %d", ctx->producer_stage);
        }
        ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
}

void producer_task(void *pvParameters){
    auto *ctx = (AppContext*)pvParameters;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task
    Sample s;
    int count = 0;
    while(1){
        LogEvent ev;
        s.count = count;
        s.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        ev.count = s.count;
        ev.timestamp_ms = s.timestamp_ms;

        if(xQueueSend(ctx->sampleQueue, &s, pdMS_TO_TICKS(50)) != pdTRUE){ //sending data
            ev.type = LogType::DROPPED;
        }
        else{
            ev.type = LogType::SENT;
        }
        if(xQueueSend(ctx->logQueue, &ev, 0) != pdTRUE){
            inc_dropped_logs(ctx);
        }
        count++;
        
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        ctx->producer_heartbeat++;

        xSemaphoreTake(ctx->settingsMutex, portMAX_DELAY);
        auto p = ctx->settings.producer_period_ms;
        xSemaphoreGive(ctx->settingsMutex);

        vTaskDelay(pdMS_TO_TICKS(p)); //delays task, ms_to_ticks converts to ms
    }
    
}

void consumer_task(void *pvParameters){
    auto *ctx = (AppContext*)pvParameters;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task
    Sample s;
    while(1){
        LogEvent ev;
        if(xQueueReceive(ctx->sampleQueue, &s, portMAX_DELAY) != pdTRUE){ 
            ev.count = 0;
            ev.timestamp_ms = 0;    
            ev.type = LogType::ERROR;
        }
        else{
            ev.count = s.count;
            ev.timestamp_ms = s.timestamp_ms;     
            ev.type = LogType::RECEIVED;
        }
        if(xQueueSend(ctx->logQueue, &ev, 0) != pdTRUE){
            inc_dropped_logs(ctx);
        }
        ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
}
