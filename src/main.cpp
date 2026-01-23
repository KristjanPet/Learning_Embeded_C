#include "freertos/FreeRTOS.h"
#include "esp_task_wdt.h"
#include "app.h"

extern "C" void app_main(void) {

    esp_task_wdt_config_t twdt_config =  {
        .timeout_ms = 5000, //5 sec
        .idle_core_mask = 0, //not watching idle tasks
        .trigger_panic = false //reset on timeout
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));

    static App app;
    app.start();

}