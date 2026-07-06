/**
  ******************************************************************************
  * @file    mq_sensors.h
  * @brief   Driver cho 3 cảm biến khí MQ-2, MQ-9, MQ-135
  * @note    Sử dụng đường cong log-log từ datasheet để tính nồng độ ppm
  *          từ tỉ số Rs/R0 (R0 = điện trở cảm biến trong không khí sạch)
  ******************************************************************************
  */
#ifndef MQ_SENSORS_H
#define MQ_SENSORS_H

#include "main.h"
#include <stdint.h>

/* ---- Cấu hình phần cứng ------------------------------------------------- */
#define MQ_VCC              5.0f      // Điện áp cấp cho cảm biến (V)
#define MQ_ADC_VREF         3.3f      // Điện áp tham chiếu ADC STM32 (V)
#define MQ_ADC_RESOLUTION   4095.0f   // ADC 12-bit
#define MQ_RL               10.0f     // Điện trở tải RL trên module (kΩ)
#define MQ_DIVIDER          1.0f      // = (R1+R2)/R2 của cầu phân áp; nối thẳng thì 1.0

/* ---- Giá trị R0 hiệu chuẩn (kΩ) - cần hiệu chuẩn lại với từng cảm biến -- */
#define MQ2_R0              9.83f     // Hiệu chuẩn trong không khí sạch
#define MQ9_R0              9.0f
#define MQ135_R0            76.63f    // MQ-135 có R0 cao hơn nhiều
extern float g_mq2_r0, g_mq9_r0, g_mq135_r0;   // R0 thật, hiệu chuẩn lúc chạy

/* ---- Kênh ADC ----------------------------------------------------------- */
#define MQ2_ADC_CHANNEL     ADC_CHANNEL_0   // PA0
#define MQ9_ADC_CHANNEL     ADC_CHANNEL_1   // PA1
#define MQ135_ADC_CHANNEL   ADC_CHANNEL_2   // PA2

/* ---- Cấu trúc dữ liệu --------------------------------------------------- */
typedef struct {
    uint16_t raw_adc;       // Giá trị ADC thô (0-4095)
    float    voltage;       // Điện áp đo (V)
    float    rs;            // Điện trở cảm biến (kΩ)
    float    ratio;         // Rs/R0
    float    ppm_lpg;       // MQ-2: LPG/propane
    float    ppm_smoke;     // MQ-2: khói
    float    ppm_h2;        // MQ-2: hydrogen
} MQ2_Data_t;

typedef struct {
    uint16_t raw_adc;
    float    voltage;
    float    rs;
    float    ratio;
    float    ppm_co;        // MQ-9: CO
    float    ppm_ch4;       // MQ-9: methane
} MQ9_Data_t;

typedef struct {
    uint16_t raw_adc;
    float    voltage;
    float    rs;
    float    ratio;
    float    ppm_co2;       // MQ-135: CO2
    float    ppm_nh3;       // MQ-135: NH3
    float    ppm_alcohol;   // MQ-135: cồn
    float    aqi_estimate;  // Ước tính chỉ số AQI
} MQ135_Data_t;

/* ---- API ---------------------------------------------------------------- */
HAL_StatusTypeDef MQ_Init(ADC_HandleTypeDef *hadc);
uint16_t          MQ_ReadADC(ADC_HandleTypeDef *hadc, uint32_t channel);
float             MQ_ADC_to_Voltage(uint16_t adc);
float             MQ_Voltage_to_Rs(float voltage);

void MQ2_Read(ADC_HandleTypeDef *hadc, MQ2_Data_t *data);
void MQ9_Read(ADC_HandleTypeDef *hadc, MQ9_Data_t *data);
void MQ135_Read(ADC_HandleTypeDef *hadc, MQ135_Data_t *data);

/* Hàm hiệu chuẩn R0 - gọi 1 lần khi cảm biến đặt trong không khí sạch */
float MQ_Calibrate_R0(ADC_HandleTypeDef *hadc, uint32_t channel,
                      float clean_air_ratio, uint16_t samples);

#endif /* MQ_SENSORS_H */
