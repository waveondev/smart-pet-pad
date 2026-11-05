#include "hx711_multi.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "smart_pad_pins.h"

static const char *TAG = "HX711_MULTI";

#define HX711_TIMEOUT_MS  70 

static const gpio_num_t data_pins[4] = {
    PIN_HX711_DATA1,
    PIN_HX711_DATA2,
    PIN_HX711_DATA3,
    PIN_HX711_DATA4
};

esp_err_t hx711_multi_init(void)
{
    gpio_config_t clk_cfg = {
        .pin_bit_mask = (1ULL << PIN_HX711_SCK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&clk_cfg);
    gpio_set_level(PIN_HX711_SCK, 0);

    uint64_t mask = 0;
    for (int i = 0; i < 4; i++) {
        mask |= (1ULL << data_pins[i]);
    }

    gpio_config_t data_cfg = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&data_cfg);

    ESP_LOGI(TAG, "HX711 4-Channel Interface initialized successfully");
    return ESP_OK;
}

bool hx711_multi_read(hx711_multi_data_t *out_data)
{
    if (out_data == NULL) {
        return false;
    }

    int elapsed_ms = 0;

    // 1. 기준 채널(DATA1) 변환 완료(LOW) 대기
    while (gpio_get_level(PIN_HX711_DATA1) != 0) {
        vTaskDelay(pdMS_TO_TICKS(2));
        elapsed_ms += 2;
        if (elapsed_ms > HX711_TIMEOUT_MS) {
            return false;
        }
    }

    // 2. 나머지 3개 채널의 변환 완료 대기 (최대 6ms 마진)
    int sync_timeout = 600; // 600 * 10us = 6ms
    while (sync_timeout > 0) {
        if (gpio_get_level(PIN_HX711_DATA2) == 0 &&
            gpio_get_level(PIN_HX711_DATA3) == 0 &&
            gpio_get_level(PIN_HX711_DATA4) == 0) {
            break; 
        }
        esp_rom_delay_us(10);
        sync_timeout--;
    }

    for (int i = 0; i < 4; i++) {
        out_data->raw[i] = 0;
    }

    // 3. 24비트 클록 쉬프트 수신
    for (int bit = 0; bit < 24; bit++) {
        gpio_set_level(PIN_HX711_SCK, 1);
        esp_rom_delay_us(1);

        for (int ch = 0; ch < 4; ch++) {
            out_data->raw[ch] = (out_data->raw[ch] << 1) | gpio_get_level(data_pins[ch]);
        }

        gpio_set_level(PIN_HX711_SCK, 0);
        esp_rom_delay_us(1);
    }

    // 4. 25번째 펄스 (Gain 128 유지)
    gpio_set_level(PIN_HX711_SCK, 1);
    esp_rom_delay_us(1);
    gpio_set_level(PIN_HX711_SCK, 0);
    esp_rom_delay_us(1);

    // 5. 부호 확장
    for (int ch = 0; ch < 4; ch++) {
        if (out_data->raw[ch] & 0x800000) {
            out_data->raw[ch] |= 0xFF000000;
        }
    }

    return true;
}