#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

struct Sample {
    int count;
    uint32_t timestamp_ms;
};

enum class LogType : uint8_t { SENT, DROPPED, RECEIVED };

struct LogEvent{
    LogType type;
    Sample s;
};

QueueHandle_t sampleQueue;
QueueHandle_t logQueue;

static const char *TAG = "MAIN";

void logger_task(void *pvParameters){
    LogEvent ev;
    while(1){
        xQueueReceive(logQueue, &ev, portMAX_DELAY);
        switch (ev.type)
        {
        case LogType::SENT:
            ESP_LOGI("LOG", "SENT count= %d, t= %u", ev.s.count, ev.s.timestamp_ms);
            break;
        case LogType::RECEIVED:
            ESP_LOGI("LOG", "RECEIVED count= %d, t= %u", ev.s.count, ev.s.timestamp_ms);
            break;
        case LogType::DROPPED:
            ESP_LOGW("LOG", "DROPPED count= %d, t= %u", ev.s.count, ev.s.timestamp_ms);
            break;
        default:
            break;
        }  
    }
}

void producer_task(void *pvParameters){
    Sample s;
    int count = 0;
    while(1){
        LogEvent ev;
        s.count = count;
        s.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        ev.s = s;
        if(xQueueSend(sampleQueue, &s, pdMS_TO_TICKS(50)) != pdTRUE){ //sending data
            ev.type = LogType::DROPPED;
        }
        else{
            ev.type = LogType::SENT;
        }
        xQueueSend(logQueue, &ev, 0);
        count++;
        vTaskDelay(pdMS_TO_TICKS(200)); //delays task, ms_to_ticks converts to ms
    }
    
}

void consumer_task(void *pvParameters){
    Sample s;
    while(1){
        LogEvent ev;
        xQueueReceive(sampleQueue, &s, portMAX_DELAY);
        ev.s = s;
        ev.type = LogType::RECEIVED;
        xQueueSend(logQueue, &ev, 0);
    }
}

extern "C" void app_main(void) {
    sampleQueue = xQueueCreate(5, sizeof(Sample)); //creates queue & and sets sizes
    if (sampleQueue == NULL){
        ESP_LOGE(TAG, "Failed to create sampleQueue");
        return;
    }
    logQueue = xQueueCreate(10, sizeof(LogEvent));
    if (logQueue == NULL){
        ESP_LOGE(TAG, "Failed to create logQueue");
        return;
    }
    xTaskCreate(logger_task, "logger", 2048, NULL, 5, NULL);
    xTaskCreate(producer_task, "producer", 2048, NULL, 3, NULL); //runs the sending function
    xTaskCreate(consumer_task, "consumer", 2048, NULL, 6, NULL); //runs the sending function
}