#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "smart_pad_pins.h"
#include "hx711_multi.h"

static const char *TAG = "SMART_CAT_PAD";

#define SAMPLE_PERIOD_MS        50 
#define KEY_LONG_PRESS_TIME_MS  3000 // 3초 이상 롱프레스 시 영점 설정

// 기본 스케일 팩터 (실제 추로 보정 가능)
#define DEFAULT_SCALE_FACTOR    25.0f

static int32_t s_tare_offset[4] = {0, 0, 0, 0};
static int32_t s_last_valid[4]  = {0, 0, 0, 0};

// 영점(Tare OFFSET) 측정 함수 (40개 샘플 약 2초간 평균)
static void perform_tare(void)
{
    ESP_LOGW(TAG, ">>> [TARE START] 패드를 빈 상태로 유지하세요 (약 2초 소요) <<<");

    int64_t sum[4] = {0};
    int count = 0;
    hx711_multi_data_t data;

    while (count < 40) {
        if (hx711_multi_read(&data)) {
            bool valid = true;
            for (int i = 0; i < 4; i++) {
                if (data.raw[i] == -1) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                for (int i = 0; i < 4; i++) {
                    sum[i] += data.raw[i];
                }
                count++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }

    for (int i = 0; i < 4; i++) {
        s_tare_offset[i] = (int32_t)(sum[i] / count);
        s_last_valid[i]  = s_tare_offset[i];
    }

    ESP_LOGW(TAG, ">>> [TARE DONE] 신규 OFFSET -> CH1: %ld | CH2: %ld | CH3: %ld | CH4: %ld <<<",
             (long)s_tare_offset[0], (long)s_tare_offset[1], 
             (long)s_tare_offset[2], (long)s_tare_offset[3]);
}

// 전면 KEY(IO2) 핀 초기화 (내부 풀업 활성화)
static void key_init(void)
{
    gpio_config_t key_cfg = {
        .pin_bit_mask = (1ULL << PIN_KEY_IN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&key_cfg);
}

void loadcell_task(void *pvParameters)
{
    hx711_multi_data_t raw_data;
    uint32_t key_pressed_duration_ms = 0;
    bool key_handled = false;

    // 부팅 직후 기본 영점 1회 취득
    perform_tare();

    ESP_LOGI(TAG, "Starting Loadcell Sampling Loop (Period: %dms)...", SAMPLE_PERIOD_MS);

    while (1) {
        // 1. IO2 KEY_IN 3초 롱프레스 감지 로직 (Active Low)
        if (gpio_get_level(PIN_KEY_IN) == 0) {
            key_pressed_duration_ms += SAMPLE_PERIOD_MS;

            // 3초 도달 시 1회 실행
            if (key_pressed_duration_ms >= KEY_LONG_PRESS_TIME_MS && !key_handled) {
                ESP_LOGI(TAG, "KEY 3초 롱프레스 감지됨! 영점(Tare)을 재설정합니다.");
                perform_tare();
                key_handled = true; // 버튼에서 손을 뗄 때까지 재실행 방지
            }
        } else {
            // 버튼을 떼면 카운터 및 플래그 리셋
            key_pressed_duration_ms = 0;
            key_handled = false;
        }

        // 2. 로드셀 데이터 읽기
        if (hx711_multi_read(&raw_data)) {
            // 결측치(-1) 필터링: -1일 경우 직전 유효값 유지
            for (int i = 0; i < 4; i++) {
                if (raw_data.raw[i] != -1) {
                    s_last_valid[i] = raw_data.raw[i];
                }
            }

            // 채널별 순수 하중(Net = 측정값 - OFFSET)
            int32_t net[4];
            int32_t total_net = 0;
            for (int i = 0; i < 4; i++) {
                net[i] = s_last_valid[i] - s_tare_offset[i];
                total_net += labs(net[i]);
            }

            // 무게(g) 환산 (50g 이하는 0g 컷)
            float total_weight_g = (float)total_net / DEFAULT_SCALE_FACTOR;
            if (total_weight_g < 50.0f) {
                total_weight_g = 0.0f;
            }

            ESP_LOGI(TAG, "Weight: %6.1fg | Net: [%6ld, %6ld, %6ld, %6ld]",
                     total_weight_g,
                     (long)net[0], (long)net[1], (long)net[2], (long)net[3]);
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing System...");

    key_init();
    ESP_ERROR_CHECK(hx711_multi_init());

    // HX711 아날로그 회로 안정화 대기
    vTaskDelay(pdMS_TO_TICKS(500));

    xTaskCreate(loadcell_task, "loadcell_task", 4096, NULL, 1, NULL);
}