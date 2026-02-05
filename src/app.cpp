#include "app.h"
#include "i2c_helper.h"
#include "spi_helper.h"

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    auto* self = static_cast<App*>(arg);
    gpio_intr_disable(GPIO_NUM_4);

    BaseType_t hpw = pdFALSE;
    auto h = self->getButtonHandle();
    if (h){
        vTaskNotifyGiveFromISR(h, &hpw);
        if (hpw) portYIELD_FROM_ISR();
    }
}

static const char *TAG = "APP";

bool App::start(){
    ctx_.dropped_logs_mux = portMUX_INITIALIZER_UNLOCKED;
    ctx_.settings.producer_period_ms = 2000;
    ctx_.stopRequested = false;

    ctx_.freeQ = xQueueCreate(POOL_N, sizeof(Sample*));
    ctx_.dataQ = xQueueCreate(POOL_N, sizeof(Sample*));
    ctx_.logQueue = xQueueCreate(10, sizeof(LogEvent));
    ctx_.buttonQ = xQueueCreate(10, sizeof(ButtonEvent));
    ctx_.cmdQ = xQueueCreate(10, sizeof(CommandEvent));
    ctx_.uiSet = xQueueCreateSet(20);

    if(!ctx_.uiSet){
        ESP_LOGE(TAG, "Failed to create uiSet");
        return false;
    }

    if (xQueueAddToSet(ctx_.buttonQ, ctx_.uiSet) != pdPASS || xQueueAddToSet(ctx_.cmdQ, ctx_.uiSet) != pdPASS) {
        ESP_LOGE(TAG, "xQueueAddToSet failed");
        return false;
    }

    //UART init
    const uart_port_t UART_NUM = UART_NUM_0;

    uart_config_t uart_cfg{};
    uart_cfg.baud_rate = 115200;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity    = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, 2048, 0, 0, nullptr, 0));

    //button setup
    gpio_config_t io_conf{};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;          // falling edge (press)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_4;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //LED setup
    gpio_config_t led_conf{};
    led_conf.intr_type = GPIO_INTR_DISABLE;
    led_conf.mode = GPIO_MODE_OUTPUT;
    led_conf.pin_bit_mask = 1ULL << GPIO_NUM_2;
    led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&led_conf));

    gpio_set_level(GPIO_NUM_2, 0); // start OFF

    //Init I2C
    ESP_LOGI(TAG, "i2c init");
    ESP_ERROR_CHECK(i2c_master_init());
    i2c_scan();

    ESP_ERROR_CHECK(ssd1306_init());
    ESP_ERROR_CHECK(ssd1306_clear());
    ESP_LOGI("OLED", "init+clear OK");

    //init SPI
    ESP_ERROR_CHECK(spl06_spi_init());

    uint8_t id = 0;
    ESP_ERROR_CHECK(spl06_read_reg(0x0D, &id));
    ESP_LOGI("SPL06", "CHIP_ID = 0x%02X", id);

    uint8_t calib[18];
    ESP_ERROR_CHECK(spl06_read_burst(0x10, calib, sizeof(calib)));

    ESP_LOGI("SPL06", "CALIB:");
    for (int i = 0; i < (int)sizeof(calib); i++) {
        ESP_LOGI("SPL06", "  0x%02X: 0x%02X", 0x10 + i, calib[i]);
    }

    //intall and register ISR
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_4, gpio_isr_handler, this));

    if(ctx_.freeQ == nullptr || ctx_.dataQ == nullptr || ctx_.logQueue == nullptr || ctx_.buttonQ == nullptr || ctx_.cmdQ == nullptr){
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

    if (xTaskCreate(&App::uart_trampoline, "uart", 3072, this, 3, &ctx_.uartHandle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create uart task"); return false;
    }

    if (xTaskCreate(&App::ui_trampoline, "ui", 2048, this, 4, &ctx_.uiHandle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ui task"); return false;
    }

    if(xTaskCreate(&App::button_trampoline, "Button", 2048, this, 4, &ctx_.buttonHandle) != pdPASS){
        ESP_LOGE(TAG, "Failed to create button task");
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

    Sample* poison = nullptr;
    LogEvent logPoison{ LogType::STOP, 0, 0};
    xQueueSend(ctx_.dataQ, &poison, 0); //send poison-pill to break task
    xQueueSend(ctx_.freeQ, &poison, 0);
    xQueueSend(ctx_.logQueue, &logPoison, 0);

    const TickType_t start = xTaskGetTickCount();

    while ((ctx_.producerHandle || ctx_.consumerHandle || ctx_.healthHandle || ctx_.loggerHandle) && (xTaskGetTickCount() - start < pdMS_TO_TICKS(2000)))
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
        
    if (ctx_.healthHandle) { //still running, force stop
        ESP_LOGE("APP", "Stop timeout: force-deleting health tasks");
        // esp_task_wdt_delete(ctx_.healthHandle);
        vTaskDelete(ctx_.healthHandle);
        ctx_.healthHandle = nullptr;
    }
    if (ctx_.consumerHandle) {
        ESP_LOGE("APP", "Stop timeout: force-deleting consumer tasks");
        // esp_task_wdt_delete(ctx_.consumerHandle);
        vTaskDelete(ctx_.consumerHandle);
        ctx_.consumerHandle = nullptr;
    }
    if (ctx_.producerHandle) {
        ESP_LOGE("APP", "Stop timeout: force-deleting producer tasks");
        // esp_task_wdt_delete(ctx_.producerHandle);
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
            if (ev.type == LogType::STOP) break;
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
                    break;
                case LogType::CHANGED:
                    ESP_LOGE("LOG", "period changed to: %d", ev.count);
                    break;
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
    // ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    uint32_t prev_hb = 0;
    uint8_t stuck_seconds = 0;

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
        if (stuck_seconds >= 6 && !ctx_.producerPaused) //reboot if stuck
        {
            ESP_LOGE("HEALTH", "Producer task stuck, rebooting task now");
            if(ctx_.producerHandle){
                // esp_task_wdt_delete(ctx_.producerHandle);
                vTaskDelete(ctx_.producerHandle);
                ctx_.producerHandle = nullptr;
            }
            xTaskCreate(&App::producer_trampoline, "producer", 2048, this, 3, &ctx_.producerHandle);
            stuck_seconds = 0;
        }
        
        // ESP_LOGI("HEALTH", "dropped_logs= %u, stage=%d", v, ctx_.producer_stage);

        // ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    // esp_task_wdt_delete(NULL);
    ctx_.healthHandle = nullptr;
    vTaskDelete(NULL);
}

void App::producer_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->producer();
}

void App::producer(){
    // ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task

    Sample *p = nullptr;
    while (true){
        if (ctx_.stopRequested) break;

        if(ctx_.producerPaused){
            vTaskDelay(pdMS_TO_TICKS(50));
            // ESP_ERROR_CHECK(esp_task_wdt_reset());
            continue;
        }

        if (xQueueReceive(ctx_.freeQ, &p, portMAX_DELAY) != pdTRUE){
            ESP_LOGE("PRODUCER", "Failed to recive freeQ data");
            continue;
        }
        else if (p == nullptr){
            break;
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
        uint32_t p = ctx_.settings.producer_period_ms;
        xSemaphoreGive(ctx_.settingsMutex);

        // ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(p));
    }
    // esp_task_wdt_delete(NULL);
    ctx_.producerHandle = nullptr;
    vTaskDelete(NULL);
}

void App::consumer_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->consumer();
}

void App::consumer(){
    // ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); //null = current task

    Sample* p = nullptr;
    while(1){
        if (ctx_.stopRequested) break;

        LogEvent ev;
        if(xQueueReceive(ctx_.dataQ, &p, pdMS_TO_TICKS(200)) == pdTRUE){ 
            if(p == nullptr) break;
            ev.count = p->count;
            ev.timestamp_ms = p->timestamp_ms;     
            ev.type = LogType::RECEIVED;
            xQueueSend(ctx_.freeQ, &p, 0);
            if(xQueueSend(ctx_.logQueue, &ev, 0) != pdTRUE){
                inc_dropped_logs();
            }
        }

        // ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
    // esp_task_wdt_delete(NULL);
    ctx_.consumerHandle = nullptr;
    vTaskDelete(NULL);
}

void App::button_trampoline(void *pv){
    auto *self = static_cast<App*>(pv);
    self->button();
}

void App::button(){
    const TickType_t debounce = pdMS_TO_TICKS(50);
    int64_t t0 = 0;
    ButtonEvent ev;

    while(true){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //waiting for ISR

        if(ctx_.stopRequested){
            gpio_intr_enable(GPIO_NUM_4);
            break;
        } 

        vTaskDelay(debounce);

        t0 = esp_timer_get_time();
        while(gpio_get_level(GPIO_NUM_4) == 0){
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (esp_timer_get_time() - t0 > 800000){
            ev = ButtonEvent::LongPress;
        }
        else{
            ev = ButtonEvent::ShortPress;
        }

        if(xQueueSend(ctx_.buttonQ, &ev, 0) != pdTRUE){
            inc_dropped_logs();
        }
        
        gpio_intr_enable(GPIO_NUM_4);
    }

    ctx_.buttonHandle = nullptr;
    vTaskDelete(NULL);
}

void App::ui_trampoline(void* pv){
    static_cast<App*>(pv)->ui_task();
}

void App::ui_task(){
    bool led = false;

    while(true){
        if(ctx_.stopRequested) break;

        QueueSetMemberHandle_t active = xQueueSelectFromSet(ctx_.uiSet, pdMS_TO_TICKS(200));

        if(active == nullptr) continue;

        if(active == ctx_.buttonQ){
            ButtonEvent ev;
            if(xQueueReceive(ctx_.buttonQ, &ev, 0) == pdTRUE){
                if(ev == ButtonEvent::ShortPress){
                    led = !led;
                    gpio_set_level(GPIO_NUM_2, led);
                    handle_toggle_period(1000);
                }
                else if(ev == ButtonEvent::LongPress){
                    handle_toggle_pause();
                }
            }
        } 
        else if(active == ctx_.cmdQ){
            CommandEvent ce;
            if(xQueueReceive(ctx_.cmdQ, &ce, 0) == pdTRUE){
                switch (ce.type)
                {
                case CommandType::SetPeriod:
                    handle_toggle_period(ce.value);
                    break;
                case CommandType::PauseToggle:
                    handle_toggle_pause();
                    break;
                case CommandType::PauseOn:
                    if(!ctx_.producerPaused){
                        handle_toggle_pause();
                    }
                    break;
                case CommandType::PauseOff:
                    if(ctx_.producerPaused){
                        handle_toggle_pause();
                    }
                    break;
                case CommandType::Status:
                    handle_status();
                    break;
                default:
                    break;
                }
            }
        }

        float t, h;
        esp_err_t e = sht31_read(&t, &h);

        memset(fb, 0, sizeof(fb));

        char line[32];
        snprintf(line, sizeof(line), "T:%.1fC H:%.0f%%", t, h);

        draw_text(0, 3, line);
        ssd1306_flush();

        if (e == ESP_OK) {
            ESP_LOGI("SHT31", "T=%.2f C  RH=%.1f %%", t, h);
        } else if (e == ESP_ERR_INVALID_CRC){
            ESP_LOGW("SHT31", "CRC error (noise on I2C?)");
        }
         else {
            ESP_LOGW("SHT31", "read failed: %s", esp_err_to_name(e));
        }
    }

    ctx_.uiHandle = nullptr;
    vTaskDelete(NULL);
}

void App::uart_trampoline(void* pv){
    static_cast<App*>(pv)->uart();
}

static char* trim(char* s) {
    // leading
    while (*s == ' ' || *s == '\t') s++;

    // trailing
    char* end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';
    return s;
}

static bool starts_with(const char* s, const char* pref){
    while (*pref) { if (*s++ != *pref++) return false; }
    return true;
}

void App::uart(){
    char buf[64];
    int len = 0;

    while(true){
        if(ctx_.stopRequested) break;

        uint8_t ch;
        CommandEvent ev;
        bool ok = true;
        int n = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(200));
        
        //normalize
        if(n != 1){
            continue;
        }

        //enter pressed
        if(ch == '\r' || ch == '\n'){
            if(len == 0) continue;
            const char nl[] = "\r\n";
            uart_write_bytes(UART_NUM_0, nl, 2);

            buf[len] = '\0';
            char* line = trim(buf);
            len = 0;

            if(!strcmp(line, "status")){
                ev.type = CommandType::Status;
            }
            else if(starts_with(line, "period ")){
                char* end = nullptr;
                unsigned long v = strtoul(line + 7, &end, 10);

                // valid only if we consumed at least 1 digit and ended at string end
                if (end == (line + 7) || *end != '\0') {
                    ok = false;
                } else {
                    ev.type = CommandType::SetPeriod;
                    ev.value = (uint32_t)v;
                }
            }
            else if(!strcmp(line, "pause toggle")){
                ev.type = CommandType::PauseToggle;
            }
            else if(!strcmp(line, "pause on")){
                ev.type = CommandType::PauseOn;
            }
            else if(!strcmp(line, "pause off")){
                ev.type = CommandType::PauseOff;
            } 
            else if(!strcmp(line, "help")){
                ESP_LOGI("UART", "Commands:");
                ESP_LOGI("UART", "  status");
                ESP_LOGI("UART", "  period <50..10000>");
                ESP_LOGI("UART", "  pause on|off|toggle");
                continue; // don’t send to cmdQ
            }
            else{
                ok = false;
            }

            if(ok){
                if(xQueueSend(ctx_.cmdQ, &ev, pdMS_TO_TICKS(50)) != pdTRUE){
                    ESP_LOGW("UART", "cmdQ full, drop");
                }
            }
            else{
                ESP_LOGW("UART", "Unknown command '%s'", line);
            }


            const char prompt[] = "> ";
            uart_write_bytes(UART_NUM_0, prompt, 2);
            continue;
        }

        if(len < (int)sizeof(buf) - 1){
            //write back
            if (ch == 0x08 || ch == 0x7F) { // backspace keys
                if (len > 0) {
                    len--;
                    // erase last char on terminal: back, space, back
                    const char bs[] = "\b \b";
                    uart_write_bytes(UART_NUM_0, bs, 3);
                }
                continue;
            }

            if (ch >= 32 && ch <= 126) { // printable ASCII
                if (len < (int)sizeof(buf) - 1) {
                    buf[len++] = (char)ch;
                    uart_write_bytes(UART_NUM_0, (const char*)&ch, 1); // echo
                } else {
                    ESP_LOGW("UART", "Line too long");
                    len = 0;
                    const char nl[] = "\r\n";
                    uart_write_bytes(UART_NUM_0, nl, 2);
                }
            }
        } 
        else{ //overflow
            len = 0;
            ESP_LOGW("UART", "line too long");
        }
    }

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

void App::handle_status() {
    uint32_t period;
    xSemaphoreTake(ctx_.settingsMutex, portMAX_DELAY);
    period = ctx_.settings.producer_period_ms;
    xSemaphoreGive(ctx_.settingsMutex);

    ESP_LOGI("STATUS", "paused=%d period_ms=%u hb=%u dropped=%u",
             (int)ctx_.producerPaused, (unsigned)period,
             (unsigned)ctx_.producer_heartbeat,
             (unsigned)get_dropped_logs());
}

void App::handle_toggle_period(uint32_t ms) {
    if(ms < 50 || ms > 10000){
        ESP_LOGW("UI", "period out of range: %u", (unsigned)ms);
        return;
    }

    xSemaphoreTake(ctx_.settingsMutex, portMAX_DELAY);
    ctx_.settings.producer_period_ms = ms;
    xSemaphoreGive(ctx_.settingsMutex);

    LogEvent le{LogType::CHANGED, (int)ms, (uint32_t)(esp_timer_get_time() / 1000)};
    if(xQueueSend(ctx_.logQueue, &le, 0) != pdTRUE){
        inc_dropped_logs();
    }  
}

void App::handle_toggle_pause() {
    ctx_.producerPaused = !ctx_.producerPaused;

    LogEvent le{};
    le.type = LogType::PAUSED;
    le.count = ctx_.producerPaused ? 1 : 0;
    le.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if(xQueueSend(ctx_.logQueue, &le, 0) != pdTRUE){
        inc_dropped_logs();
    }  
}
