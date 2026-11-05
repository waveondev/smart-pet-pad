#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t raw[4];
} hx711_multi_data_t;

esp_err_t hx711_multi_init(void);
bool hx711_multi_read(hx711_multi_data_t *out_data);

#ifdef __cplusplus
}
#endif