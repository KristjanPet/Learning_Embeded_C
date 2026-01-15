#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

struct Sample {
    int count;
    uint32_t timestamp_ms;
};

QueueHandle_t sampleQueue;

static const char *TAG = "MAIN";

void producer_task(void *pvParameters){
    Sample s;
    int count = 0;
    while(1){
        s.count = count;
        s.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if(xQueueSend(sampleQueue, &s, pdMS_TO_TICKS(50)) != pdTRUE){ //sending data
            ESP_LOGW("sender", "queue full at: %d", s.count);
        }
        else{
            ESP_LOGI("sender", "sent: %d", s.count);
        }
        count++;
        vTaskDelay(pdMS_TO_TICKS(200)); //delays task, ms_to_ticks converts to ms
    }
    
}

void consumer_task(void *pvParameters){
    Sample s;
    while(1){
        if(xQueueReceive(sampleQueue, &s, portMAX_DELAY) != pdTRUE){
            ESP_LOGE("consumer", "receiving data failed");
        };
        ESP_LOGI("consumer", "got: %d with time: %u", s.count, s.timestamp_ms);
    }
}

extern "C" void app_main(void) {
    sampleQueue = xQueueCreate(5, sizeof(Sample)); //creates queue
    if (sampleQueue == NULL){
        ESP_LOGE(TAG, "Failed to create sampleQueue");
        return;
    }
    xTaskCreate(producer_task, "producer", 2048, NULL, 3, NULL); //runs the sending function
    xTaskCreate(consumer_task, "consumer", 2048, NULL, 6, NULL); //runs the sending function
}