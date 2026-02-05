#include "spi_helper.h"

esp_err_t spl06_spi_init(){
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = GPIO_NUM_23;
    buscfg.miso_io_num = GPIO_NUM_19;
    buscfg.sclk_io_num = GPIO_NUM_18;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;

    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 1 * 1000 * 1000;   // 1 MHz safe start
    devcfg.mode = 0;                           // SPI mode 0
    devcfg.spics_io_num = GPIO_NUM_5;          // CSB
    devcfg.queue_size = 1;

    return spi_bus_add_device(VSPI_HOST, &devcfg, &spl06_dev);
}

esp_err_t spl06_read_reg(uint8_t reg, uint8_t *out){
    uint8_t tx[2] = { (uint8_t)((reg & 0x7F) | 0x80), 0x00 };
    uint8_t rx[2] = {0};

    spi_transaction_t t = {};
    t.length = 8 * sizeof(tx);
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t err = spi_device_transmit(spl06_dev, &t);
    if (err != ESP_OK) return err;

    *out = rx[1]; //first byte is control phase
    return ESP_OK;
}

esp_err_t spl06_read_burst(uint8_t start_reg, uint8_t *out, size_t n){
    const size_t total = n + 1;

    uint8_t tx[1 + 32]; // up to 32 bytes burst here
    uint8_t rx[1 + 32];
    if (total > sizeof(tx)) return ESP_ERR_INVALID_SIZE;

    tx[0] = (uint8_t)((start_reg & 0x7F) | 0x80);
    memset(&tx[1], 0x00, n);

    spi_transaction_t t = {};
    t.length = 8 * total;
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t err = spi_device_transmit(spl06_dev, &t);
    if (err != ESP_OK) return err;

    memcpy(out, &rx[1], n);
    return ESP_OK;
}

esp_err_t spl06_write_reg(uint8_t reg, uint8_t val){
    uint8_t tx[2] = { (uint8_t)(reg & 0x7F), val };
    spi_transaction_t t = {};
    t.length = 8 * sizeof(tx);
    t.tx_buffer = tx;
    return spi_device_transmit(spl06_dev, &t);
}
