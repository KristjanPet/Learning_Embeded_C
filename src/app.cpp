#include "app.h"

static const char *TAG = "APP";

bool App::start(){
    ctx_.dropped_logs_mux = portMUX_INITIALIZER_UNLOCKED;
    ctx_.settings.producer_period_ms = 200;
    ctx_.stopRequested = false;

    ctx_.freeQ = xQueueCreate(POOL_N, sizeof(Sample*));
    ctx_.dataQ = xQueueCreate(POOL_N, sizeof(Sample*));
    ctx_.logQueue = xQueueCreate(10, sizeof(LogEvent));

    if(ctx_.freeQ == nullptr || ctx_.dataQ == nullptr || ctx_.logQueue == nullptr){
        ESP_LOGE(TAG, "Failed to create Queue");
        return false;
    }

    for(int i = 0; i < POOL_N; i++){
        Sample* p = &pool_[i];
        if (xQueueSend(ctx_.freeQ, &p, 0) != pdTRUE){
            ESP_LOGE("INIT", "Failed to init freeQ queue");
            return false;
        }
    }

    ctx_.settingsMutex = xSemaphoreCreateMutex();
    if (ctx_.settingsMutex == NULL){
        ESP_LOGE("INIT", "Failed to create settings Mutex");
        return false;
    }

    if (xTaskCreate(&App::health_trampoline, "health", 2048, this, 2, &ctx_.healthHandle) != pdPASS){
        ESP_LOGE(TAG, "Failed to create health task");
        return false;
    }

    if (xTaskCreate(&App::logger_trampoline, "logger", 2048, this, 5, &ctx_.loggerHandle) != pdPASS){
        ESP_LOGE(TAG, "Failed to create logger task");
        return false;
    }

    if (xTaskCreate(&App::producer_trampoline, "producer", 2048, this, 3, &ctx_.producerHandle) != pdPASS){
        ESP_LOGE(TAG, "Failed to create producer task");
        return false;
    }

    if (xTaskCreate(&App::consumer_trampoline, "consumer", 2048, this, 6, &ctx_.consumerHandle) != pdPASS){
        ESP_LOGE(TAG, "Failed to create consumer task");
        return false;
    }

    return true;
}

bool App::stop(){
    ctx_.stopRequested = true;
    const TickType_t start = xTaskGetTickCount();

    while ((ctx_.producerHandle || ctx_.consumerHandle || ctx_.healthHandle || ctx_.loggerHandle) && (xTaskGetTickCount() - start < pdMS_TO_TICKS(2000)))
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
        
    if (ctx_.healthHandle) { //still running, force stop
        ESP_LOGE("APP", "Stop timeout: force-deleting health tasks");
        esp_task_wdt_delete(ctx_.healthHandle);
        vTaskDelete(ctx_.healthHandle);
        ctx_.healthHandle = nullptr;
    }
    if (ctx_.consumerHandle) {
        ESP_LOGE("APP", "Stop timeout: force-deleting consumer tasks");
        esp_task_wdt_delete(ctx_.consumerHandle);
        vTaskDelete(ctx_.consumerHandle);
        ctx_.consumerHandle = nullptr;
    }
    if (ctx_.producerHandle) {
        ESP_LOGE("APP", "Stop timeout: force-deleting producer tasks");
        esp_task_wdt_delete(ctx_.producerHandle);
        vTaskDelete(ctx_.producerHandle);
        ctx_.producerHandle = nullptr;
    }
    if (ctx_.loggerHandle) {
        ESP_LOGE("APP", "Stop timeout: force-deleting logger tasks");
        vTaskDelete(ctx_.loggerHandle);
        ctx_.loggerHandle = nullptr;
    }

    if(ctx_.freeQ){
        vQueueDelete(ctx_.freeQ);
        ctx_.freeQ = nullptr;
    }

    if(ctx_.dataQ){
        vQueueDelete(ctx_.dataQ);
        ctx_.dataQ = nullptr;
    }

    if(ctx_.logQueue){
        vQueueDelete(ctx_.logQueue);
        ctx_.logQueue = nullptr;
    }

    if(ctx_.settingsMutex){
        vSemaphoreDelete(ctx_.settingsMutex);
        ctx_.settingsMutex = nullptr;
    }

    return true;
}

void App::logger_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->logger();
}

void App::logger(){
    LogEvent ev;
    while (true){
        if (ctx_.stopRequested) break;
        if(xQueueReceive(ctx_.logQueue, &ev, portMAX_DELAY) != pdTRUE){
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
    }
    ctx_.loggerHandle = nullptr;
    vTaskDelete(NULL);
}

void App::health_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->health();
}

void App::health(){
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    uint32_t prev_hb = 0;
    uint8_t stuck_seconds = 0;

    int toggleSendPeriod = 0;

    while(1){
        if (ctx_.stopRequested) break;
        uint32_t v = get_dropped_logs();
        if (ctx_.producer_heartbeat == prev_hb){ //check if stuck
            stuck_seconds++;
        }
        else {
            stuck_seconds = 0;
        }
        prev_hb = ctx_.producer_heartbeat;
        if (stuck_seconds >= 6) //reboot if stuck
        {
            ESP_LOGE("HEALTH", "Producer task stuck, rebooting task now");
            if(ctx_.producerHandle){
                esp_task_wdt_delete(ctx_.producerHandle);
                vTaskDelete(ctx_.producerHandle);
                ctx_.producerHandle = nullptr;
            }
            xTaskCreate(&App::producer_trampoline, "producer", 2048, this, 3, &ctx_.producerHandle);
            stuck_seconds = 0;
        }
        
        ESP_LOGI("HEALTH", "dropped_logs= %u, stage=%d", v, ctx_.producer_stage);

        //toggle send period for mutex
        if (toggleSendPeriod >= 5){
            xSemaphoreTake(ctx_.settingsMutex, portMAX_DELAY);
            ctx_.settings.producer_period_ms = (ctx_.settings.producer_period_ms == 200) ? 200 : 200;
            xSemaphoreGive(ctx_.settingsMutex);
            toggleSendPeriod = 0;
        }
        toggleSendPeriod++;

        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    esp_task_wdt_delete(NULL);
    ctx_.healthHandle = nullptr;
    vTaskDelete(NULL);
}

void App::producer_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->producer();
}

void App::producer(){
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task

    Sample *p = nullptr;
    while (true){
        if (ctx_.stopRequested) break;

        if (xQueueReceive(ctx_.freeQ, &p, portMAX_DELAY) != pdTRUE){
            ESP_LOGE("PRODUCER", "Failed to recive freeQ data");
            continue;
        }

        LogEvent ev;
        p->count = ctx_.producer_heartbeat;
        p->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        ev.count = p->count;
        ev.timestamp_ms = p->timestamp_ms;

        if(xQueueSend(ctx_.dataQ, &p, portMAX_DELAY) != pdTRUE){ //sending data
            xQueueSend(ctx_.freeQ, &p, 0);
            ev.type = LogType::ERROR;
        }
        else{
            ev.type = LogType::SENT;
            ctx_.producer_heartbeat++;
        }
        if(xQueueSend(ctx_.logQueue, &ev, 0) != pdTRUE){
            inc_dropped_logs();
        }   

        //Testing mutex
        xSemaphoreTake(ctx_.settingsMutex, portMAX_DELAY);
        auto p = ctx_.settings.producer_period_ms;
        xSemaphoreGive(ctx_.settingsMutex);

        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(p));
    }
    esp_task_wdt_delete(NULL);
    ctx_.producerHandle = nullptr;
    vTaskDelete(NULL);
}

void App::consumer_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->consumer();
}

void App::consumer(){
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task

    Sample* p = nullptr;
    while(1){
        if (ctx_.stopRequested) break;

        LogEvent ev;
        if(xQueueReceive(ctx_.dataQ, &p, portMAX_DELAY) != pdTRUE){ 
            ev.count = 0;
            ev.timestamp_ms = 0;    
            ev.type = LogType::ERROR;
        }
        else{
            ev.count = p->count;
            ev.timestamp_ms = p->timestamp_ms;     
            ev.type = LogType::RECEIVED;
            xQueueSend(ctx_.freeQ, &p, 0);
        }
        if(xQueueSend(ctx_.logQueue, &ev, 0) != pdTRUE){
            inc_dropped_logs();
        }

        ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
    esp_task_wdt_delete(NULL);
    ctx_.consumerHandle = nullptr;
    vTaskDelete(NULL);
}

void App::inc_dropped_logs(){
    portENTER_CRITICAL(&ctx_.dropped_logs_mux);
    ctx_.dropped_logs++;
    portEXIT_CRITICAL(&ctx_.dropped_logs_mux);
}

uint32_t App::get_dropped_logs() {
    portENTER_CRITICAL(&ctx_.dropped_logs_mux);
    uint32_t v = ctx_.dropped_logs;
    portEXIT_CRITICAL(&ctx_.dropped_logs_mux);
    return v;
}