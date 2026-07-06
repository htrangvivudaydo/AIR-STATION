/**
  ******************************************************************************
  * @file    gp2y1010.h
  * @brief   Driver cảm biến bụi quang học Sharp GP2Y1010AU0F
  * @note    Cảm biến cần xung điều khiển LED hồng ngoại theo thời gian chính xác:
  *          - Kéo LED LOW (bật LED)
  *          - Chờ 280 μs
  *          - Đọc ADC tại chân Vo
  *          - Chờ 40 μs
  *          - Kéo LED HIGH (tắt LED)
  *          - Chu kỳ tổng ~10 ms
  ******************************************************************************
  */
#ifndef GP2Y1010_H
#define GP2Y1010_H

#include "main.h"
#include <stdint.h>

/* ---- Cấu hình phần cứng ------------------------------------------------- */
#define GP2Y_ADC_CHANNEL        ADC_CHANNEL_3   // PA3
#define GP2Y_LED_GPIO_Port      GPIOA
#define GP2Y_LED_Pin            GPIO_PIN_4      // PA4 (active LOW)

#define GP2Y_ADC_VREF           3.3f
#define GP2Y_ADC_RESOLUTION     4095.0f

/* Hệ số chuyển đổi điện áp -> mật độ bụi (mg/m³)
 * Theo datasheet: độ nhạy ~0.5 V / (0.1 mg/m³)
 *                 điện áp offset không bụi ~0.6 V
 * Công thức:   density = (V - V_offset) / sensitivity
 *              density (μg/m³) = (V - 0.6) * 200   (nếu V > 0.6)
 */
#define GP2Y_SENSITIVITY        0.005f  // V per μg/m³
#define GP2Y_GAIN  1.0f   // = nghịch đảo tỉ số chia áp. Nối thẳng Vo->ADC thì = 1.0
extern float g_gp2y_v_clean;   // V trong không khí sạch


/* ---- Cấu trúc dữ liệu --------------------------------------------------- */
typedef struct {
    uint16_t raw_adc;           // Giá trị ADC thô
    float    voltage;           // Điện áp đo (V)
    float    dust_ug_m3;        // Mật độ bụi μg/m³
    float    dust_mg_m3;        // Mật độ bụi mg/m³
    uint8_t  air_quality_level; // 1=Tốt, 2=TB, 3=Kém, 4=Xấu, 5=Nguy hiểm
} GP2Y_Data_t;

/* ---- API ---------------------------------------------------------------- */
void GP2Y_Init(void);
float GP2Y_CalibrateBaseline(ADC_HandleTypeDef *hadc, uint16_t n);
void GP2Y_Read(ADC_HandleTypeDef *hadc, GP2Y_Data_t *data);
const char* GP2Y_GetQualityString(uint8_t level);

#endif /* GP2Y1010_H */
