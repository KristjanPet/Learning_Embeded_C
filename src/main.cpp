#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

QueueHandle_t sampleQueue;

static const char *TAG = "MAIN";

void producer_task(void *pvParameters){
    int count = 0;
    while(1){
        if(xQueueSend(sampleQueue, &count, pdMS_TO_TICKS(50)) != pdTRUE){ //sending data
            ESP_LOGI("sender", "queue full at: %d", count);
        }
        else{
            ESP_LOGI("sender", "sent: %d", count);
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); //delays task, ms_to_ticks converts to ms
    }
    
}

void consumer_task(void *pvParameters){
    int value;
    while(1){
        if(xQueueReceive(sampleQueue, &value, portMAX_DELAY) != pdTRUE){
            ESP_LOGE("consumer", "receiving data failed");
        };
        ESP_LOGI("consumer", "got: %d", value);
    }
}

extern "C" void app_main(void) {
    sampleQueue = xQueueCreate(5, sizeof(int)); //creates queue
    if (sampleQueue == NULL){
        ESP_LOGE(TAG, "Failed to create sampleQueue");
        return;
    }
    xTaskCreate(producer_task, "producer", 2048, NULL, 3, NULL); //runs the sending function
    xTaskCreate(consumer_task, "consumer", 2048, NULL, 6, NULL); //runs the sending function
}