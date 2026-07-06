/**
  ******************************************************************************
  * @file    mq_sensors.c
  * @brief   Triển khai driver cho MQ-2, MQ-9, MQ-135
  * @note    Công thức tính ppm: ppm = a * (Rs/R0)^b
  *          Hệ số a, b được lấy từ datasheet (đường cong log-log)
  ******************************************************************************
  */
#include "mq_sensors.h"
#include <math.h>
#define MQ2_ADC_CHANNEL   ADC_CHANNEL_0
#define MQ9_ADC_CHANNEL   ADC_CHANNEL_1
#define MQ135_ADC_CHANNEL ADC_CHANNEL_2
float g_mq2_r0 = MQ2_R0, g_mq9_r0 = MQ9_R0, g_mq135_r0 = MQ135_R0;
/* =============================================================================
 *  Hàm tiện ích chung
 * ===========================================================================*/

/**
 * @brief  Chọn kênh ADC và đọc 1 mẫu (polling mode)
 */
uint16_t MQ_ReadADC(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;  // Tăng thời gian lấy mẫu để ổn định

    if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK) {
        return 0;
    }

    HAL_ADC_Start(hadc);
    if (HAL_ADC_PollForConversion(hadc, HAL_MAX_DELAY ) != HAL_OK) {
        HAL_ADC_Stop(hadc);
        return 0;
    }
    uint16_t value = (uint16_t)HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);

    return value;
}

float MQ_ADC_to_Voltage(uint16_t adc)
{
    return ((float)adc * MQ_ADC_VREF) / MQ_ADC_RESOLUTION;
}

/**
 * @brief  Tính điện trở Rs của cảm biến từ điện áp đo được
 *         Rs = ((Vcc - Vout) / Vout) * RL
 *         Lưu ý: Vout đo từ chân AO của module (đã chia qua RL)
 */
float MQ_Voltage_to_Rs(float v_adc) {
    float v_ao = v_adc * MQ_DIVIDER;            // điện áp thật tại chân AO
    if (v_ao < 0.01f) return 0.0f;
    return ((MQ_VCC - v_ao) / v_ao) * MQ_RL;    // Rs (kΩ)
}
/* =============================================================================
 *  Khởi tạo
 * ===========================================================================*/
HAL_StatusTypeDef MQ_Init(ADC_HandleTypeDef *hadc)
{
    /* Hiệu chỉnh ADC (chỉ cần với F1/L0/L4, F4 không cần nhưng vẫn an toàn) */
    return HAL_OK;
}

/* =============================================================================
 *  MQ-2: phát hiện LPG, propane, khói, hydrogen
 *  Đường cong datasheet (Rs/R0 vs ppm):
 *    LPG:    a = 574.25,  b = -2.222
 *    Smoke:  a = 3616.1,  b = -2.675
 *    H2:     a = 987.99,  b = -2.162
 * ===========================================================================*/
void MQ2_Read(ADC_HandleTypeDef *hadc, MQ2_Data_t *data)
{
    data->raw_adc = MQ_ReadADC(hadc, MQ2_ADC_CHANNEL);
    data->voltage = MQ_ADC_to_Voltage(data->raw_adc);
    data->rs      = MQ_Voltage_to_Rs(data->voltage);
    data->ratio   = data->rs / g_mq2_r0;

    if (data->ratio <= 0.0f) {
        data->ppm_lpg = data->ppm_smoke = data->ppm_h2 = 0.0f;
        return;
    }
    data->ppm_lpg   = 574.25f  * powf(data->ratio, -2.222f);
    data->ppm_smoke = 3616.1f  * powf(data->ratio, -2.675f);
    data->ppm_h2    = 987.99f  * powf(data->ratio, -2.162f);
}

/* =============================================================================
 *  MQ-9: phát hiện CO và CH4
 *  CO:   a = 1000,   b = -1.4
 *  CH4:  a = 4000,   b = -2.0
 * ===========================================================================*/
void MQ9_Read(ADC_HandleTypeDef *hadc, MQ9_Data_t *data)
{
    data->raw_adc = MQ_ReadADC(hadc, MQ9_ADC_CHANNEL);
    data->voltage = MQ_ADC_to_Voltage(data->raw_adc);
    data->rs      = MQ_Voltage_to_Rs(data->voltage);
    data->ratio   = data->rs / g_mq9_r0;

    if (data->ratio <= 0.0f) {
        data->ppm_co = data->ppm_ch4 = 0.0f;
        return;
    }
    data->ppm_co  = 1000.0f * powf(data->ratio, -1.4f);
    data->ppm_ch4 = 4000.0f * powf(data->ratio, -2.0f);
}

/* =============================================================================
 *  MQ-135: phát hiện CO2, NH3, cồn, benzene
 *  CO2:     a = 110.47,  b = -2.862
 *  NH3:     a = 102.2,   b = -2.473
 *  Alcohol: a = 77.255,  b = -3.18
 * ===========================================================================*/
void MQ135_Read(ADC_HandleTypeDef *hadc, MQ135_Data_t *data)
{
    data->raw_adc = MQ_ReadADC(hadc, MQ135_ADC_CHANNEL);
    data->voltage = MQ_ADC_to_Voltage(data->raw_adc);
    data->rs      = MQ_Voltage_to_Rs(data->voltage);
    data->ratio   = data->rs / g_mq135_r0;

    if (data->ratio <= 0.0f) {
        data->ppm_co2 = data->ppm_nh3 = data->ppm_alcohol = 0.0f;
        data->aqi_estimate = 0.0f;
        return;
    }
    data->ppm_co2     = 110.47f * powf(data->ratio, -2.862f);
    data->ppm_nh3     = 102.2f  * powf(data->ratio, -2.473f);
    data->ppm_alcohol = 77.255f * powf(data->ratio, -3.18f);

    /* AQI ước tính dựa trên CO2 (đơn giản hoá):
     *   < 400 ppm  -> tốt (0-50)
     *   400-1000   -> trung bình (50-100)
     *   1000-2000  -> kém (100-150)
     *   > 2000     -> rất kém (>150)
     */
    if      (data->ppm_co2 < 400.0f)  data->aqi_estimate = data->ppm_co2 / 8.0f;
    else if (data->ppm_co2 < 1000.0f) data->aqi_estimate = 50.0f + (data->ppm_co2 - 400.0f) / 12.0f;
    else if (data->ppm_co2 < 2000.0f) data->aqi_estimate = 100.0f + (data->ppm_co2 - 1000.0f) / 20.0f;
    else                              data->aqi_estimate = 150.0f + (data->ppm_co2 - 2000.0f) / 40.0f;
}

/* =============================================================================
 *  Hiệu chuẩn R0 trong không khí sạch
 *  clean_air_ratio: tỉ số Rs/R0 trong không khí sạch (từ datasheet)
 *      MQ-2:   9.83
 *      MQ-9:   9.0   (~9.5 thường được dùng)
 *      MQ-135: 3.6
 * ===========================================================================*/
float MQ_Calibrate_R0(ADC_HandleTypeDef *hadc, uint32_t channel,
                      float clean_air_ratio, uint16_t samples)
{
    float rs_sum = 0.0f;
    for (uint16_t i = 0; i < samples; i++) {
        uint16_t adc = MQ_ReadADC(hadc, channel);
        float v  = MQ_ADC_to_Voltage(adc);
        float rs = MQ_Voltage_to_Rs(v);
        rs_sum += rs;
        HAL_Delay(50);
    }
    float rs_avg = rs_sum / (float)samples;
    return rs_avg / clean_air_ratio;
}
