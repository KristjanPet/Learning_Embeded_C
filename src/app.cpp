#include "app.h"

static const char *TAG = "APP";

bool App::start(){
    ctx_.dropped_logs_mux = portMUX_INITIALIZER_UNLOCKED;
    ctx_.dropped_logs = 0;
    ctx_.producer_stage = 0;
    ctx_.producer_heartbeat = 0;

    ctx_.sampleQueue = xQueueCreate(5, sizeof(Sample));
    ctx_.logQueue = xQueueCreate(10, sizeof(LogEvent));

    if(ctx_.sampleQueue == nullptr || ctx_.logQueue == nullptr){
        ESP_LOGE(TAG, "Failed to create Queue");
        return false;
    }

    if (xTaskCreate(&App::producer_trampoline, "producer", 2048, this, 3, &ctx_.producerHandle) != pdPASS){
        ESP_LOGE(TAG, "Failed to create producer task");
        return false;
    }

    if (xTaskCreate(&App::logger_trampoline, "logger", 2048, this, 5, NULL) != pdPASS){
        ESP_LOGE(TAG, "Failed to create logger task");
        return false;
    }

    if (xTaskCreate(&App::consumer_trampoline, "consumer", 2048, this, 6, NULL) != pdPASS){
        ESP_LOGE(TAG, "Failed to create consumer task");
        return false;
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
}

void App::producer_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->producer();
}

void App::producer(){
    Sample s;
    while (true){
        LogEvent ev;
        s.count = ctx_.producer_heartbeat;
        s.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        ev.count = s.count;
        ev.timestamp_ms = s.timestamp_ms;

        if(xQueueSend(ctx_.sampleQueue, &s, pdMS_TO_TICKS(50)) != pdTRUE){ //sending data
            ev.type = LogType::DROPPED;
        }
        else{
            ev.type = LogType::SENT;
        }
        if(xQueueSend(ctx_.logQueue, &ev, 0) != pdTRUE){
            inc_dropped_logs();
        }

        ctx_.producer_heartbeat++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void App::consumer_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->consumer();
}

void App::consumer(){
    Sample s;
    while(1){
        LogEvent ev;
        if(xQueueReceive(ctx_.sampleQueue, &s, portMAX_DELAY) != pdTRUE){ 
            ev.count = 0;
            ev.timestamp_ms = 0;    
            ev.type = LogType::ERROR;
        }
        else{
            ev.count = s.count;
            ev.timestamp_ms = s.timestamp_ms;     
            ev.type = LogType::RECEIVED;
        }
        if(xQueueSend(ctx_.logQueue, &ev, 0) != pdTRUE){
            inc_dropped_logs();
        }
    }
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