#include "spi_helper.h"

Spl06Cal spl_cal;

esp_err_t spl06_spi_init(){

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

    tx[0] = (uint8_t)((start_reg & 0x7F) | 0x80);   //reg & 0x7F clears bit7 (forces it 0), | 0x80 sets bit7 to 1 
    memset(&tx[1], 0x00, n);                        //bit7 = 1 → READ, bit7 = 0 → WRITE

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

static int32_t sign_extend(int32_t v, int bits) { // converts a value with given bits (e.g. 12 or 20) to a proper signed 32-bit integer
    const int32_t shift = 32 - bits;    // if bits=12, shift=20; if bits=20, shift=12
    return (v << shift) >> shift;       // first bit defines the sign, so move it to the leftmost position, then move back to the right, filling with the sign bit on the left
}

void spl06_parse_calib(const uint8_t b[18], Spl06Cal *c){
    // c0, c1 are 12-bit signed
    int32_t c0 = (int32_t)((b[0] << 4) | (b[1] >> 4));
    int32_t c1 = (int32_t)(((b[1] & 0x0F) << 8) | b[2]);
    c->c0 = sign_extend(c0, 12);
    c->c1 = sign_extend(c1, 12);

    // c00, c10 are 20-bit signed
    int32_t c00 = (int32_t)((b[3] << 12) | (b[4] << 4) | (b[5] >> 4)); //combines together byte 3 (8 bits moved to the front), byte 4 (8 bits in the middle), and the first 4 bits of byte 5
    int32_t c10 = (int32_t)(((b[5] & 0x0F) << 16) | (b[6] << 8) | b[7]);
    c->c00 = sign_extend(c00, 20);
    c->c10 = sign_extend(c10, 20);

    // The rest are 16-bit signed
    c->c01 = sign_extend((int32_t)((b[8]  << 8) | b[9]), 16);
    c->c11 = sign_extend((int32_t)((b[10] << 8) | b[11]), 16);
    c->c20 = sign_extend((int32_t)((b[12] << 8) | b[13]), 16);
    c->c21 = sign_extend((int32_t)((b[14] << 8) | b[15]), 16);
    c->c30 = sign_extend((int32_t)((b[16] << 8) | b[17]), 16);
}

static float spl06_scale_from_osr(uint8_t osr){
    // Common SPL06 scale factors (datasheet table)
    switch (osr) {
        case 0: return 524288.0f;  // 1x
        case 1: return 1572864.0f; // 2x
        case 2: return 3670016.0f; // 4x
        case 3: return 7864320.0f; // 8x
        case 4: return 253952.0f;  // 16x
        case 5: return 516096.0f;  // 32x
        case 6: return 1040384.0f; // 64x
        case 7: return 2088960.0f; // 128x
        default: return 524288.0f;
    }
}

void spl06_compensate(int32_t p_raw, int32_t t_raw,
                             uint8_t prs_cfg, uint8_t tmp_cfg,
                             float *temp_c, float *press_pa){
    uint8_t p_osr = (prs_cfg & 0x07); // oversampling bits (common)
    uint8_t t_osr = (tmp_cfg & 0x07);

    float kP = spl06_scale_from_osr(p_osr);
    float kT = spl06_scale_from_osr(t_osr);

    float Tsc = (float)t_raw / kT;
    float Psc = (float)p_raw / kP;

    *temp_c = (float)spl_cal.c0 * 0.5f + (float)spl_cal.c1 * Tsc;

    float P = (float)spl_cal.c00
            + Psc * ((float)spl_cal.c10 + Psc * ((float)spl_cal.c20 + Psc * (float)spl_cal.c30))
            + Tsc * (float)spl_cal.c01
            + Tsc * Psc * ((float)spl_cal.c11 + Psc * (float)spl_cal.c21);

    *press_pa = P; // in Pa (per formula)
}

float altitude_from_hpa(float p_hpa, float p0_hpa)
{
    // avoid divide-by-zero / nonsense
    if (p0_hpa <= 0.0f || p_hpa <= 0.0f) return 0.0f;

    const float ratio = p_hpa / p0_hpa;
    return 44330.0f * (1.0f - powf(ratio, 0.1903f));
}
