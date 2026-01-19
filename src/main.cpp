#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

struct Sample {
    int count;
    uint32_t timestamp_ms;
};

enum class LogType : uint8_t { SENT, DROPPED, RECEIVED, ERROR };

struct LogEvent{
    LogType type;
    int count;
    uint32_t timestamp_ms;
};

QueueHandle_t sampleQueue;
QueueHandle_t logQueue;

void producer_task(void *pvParameters);

uint32_t dropped_logs = 0;
volatile int producer_stage = 0;

static const char *TAG = "MAIN";
static portMUX_TYPE dropped_logs_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t producerHandle = nullptr;


static inline void inc_dropped_logs() {
  portENTER_CRITICAL(&dropped_logs_mux);
  dropped_logs++;
  portEXIT_CRITICAL(&dropped_logs_mux);
}

static inline uint32_t get_dropped_logs() {
  portENTER_CRITICAL(&dropped_logs_mux);
  uint32_t v = dropped_logs;
  portEXIT_CRITICAL(&dropped_logs_mux);
  return v;
}

void health_task(void *pvParameters){
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    int prev_stage = 0;
    uint8_t stuck_seconds = 0;
    while(1){
        uint32_t v = get_dropped_logs();
        if (producer_stage == prev_stage){ //check if stuck
            stuck_seconds++;
        }
        else {
            stuck_seconds = 0;
        }
        prev_stage = producer_stage;
        if (stuck_seconds >= 6) //reboot if stuck
        {
            ESP_LOGE("HEALTH", "Producer task stuck, rebooting task now");
            if(producerHandle){
                vTaskDelete(producerHandle);
                producerHandle = nullptr;
            }
            xTaskCreate(producer_task, "producer", 2048, NULL, 3, &producerHandle);
            stuck_seconds = 0;
        }
        
        ESP_LOGI("HEALTH", "dropped_logs= %u, stage=%d", v, producer_stage);
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void logger_task(void *pvParameters){
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task
    LogEvent ev;
    int n = 0;
    while(1){
        if(xQueueReceive(logQueue, &ev, portMAX_DELAY) != pdTRUE){
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
            ESP_LOGI("LOG", "producer stage: %d", producer_stage);
        }
        ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
}

void producer_task(void *pvParameters){
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task
    Sample s;
    int count = 0;
    while(1){
        LogEvent ev;
        s.count = count;
        s.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        ev.count = s.count;
        ev.timestamp_ms = s.timestamp_ms;
        producer_stage = 1;
        if(xQueueSend(sampleQueue, &s, pdMS_TO_TICKS(50)) != pdTRUE){ //sending data
            ev.type = LogType::DROPPED;
        }
        else{
            ev.type = LogType::SENT;
        }
        producer_stage = 21;
        if(xQueueSend(logQueue, &ev, 0) != pdTRUE){
            inc_dropped_logs();
        }
        producer_stage = 22;
        count++;
        producer_stage = 30;
        if (count == 50)
        {
            producer_stage = 31;
            while (1)
            {
                //testing watchdog reboot
            }
            
        }
        
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        producer_stage = 31;
        vTaskDelay(pdMS_TO_TICKS(200)); //delays task, ms_to_ticks converts to ms
    }
    
}

void consumer_task(void *pvParameters){
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task
    Sample s;
    while(1){
        LogEvent ev;
        if(xQueueReceive(sampleQueue, &s, portMAX_DELAY) != pdTRUE){ 
            ev.count = 0;
            ev.timestamp_ms = 0;    
            ev.type = LogType::ERROR;
        }
        else{
            ev.count = s.count;
            ev.timestamp_ms = s.timestamp_ms;     
            ev.type = LogType::RECEIVED;
        }
        if(xQueueSend(logQueue, &ev, 0) != pdTRUE){
            inc_dropped_logs();
        }
        ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
}

extern "C" void app_main(void) {
    esp_task_wdt_config_t twdt_config =  {
        .timeout_ms = 5000, //5 sec
        .idle_core_mask = 0, //not watching idle tasks
        .trigger_panic = false //reset on timeout
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
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
    xTaskCreate(health_task, "health", 2048, NULL, 2, NULL);
    xTaskCreate(logger_task, "logger", 2048, NULL, 5, NULL);
    xTaskCreate(producer_task, "producer", 2048, NULL, 3, &producerHandle); //runs the sending task
    xTaskCreate(consumer_task, "consumer", 2048, NULL, 6, NULL); //runs the sending task
}