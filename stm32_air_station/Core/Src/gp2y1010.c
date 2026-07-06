/**
  ******************************************************************************
  * @file    gp2y1010.c
  * @brief   Triển khai driver GP2Y1010AU0F
  ******************************************************************************
  */
#include "gp2y1010.h"
#include "mq_sensors.h"   // dùng lại hàm MQ_ReadADC

/* Delay μs đơn giản dùng vòng lặp NOP (không chính xác tuyệt đối nhưng đủ dùng
 * ở mức μs cho mục đích này). Với SYSCLK = 100 MHz, mỗi lần lặp ~ vài ns,
 * cần ~100 lần lặp cho 1μs.
 * Khuyến nghị thay bằng DWT->CYCCNT nếu cần chính xác hơn.
 */
static void delay_us(uint32_t us)
{
    /* Giả sử SYSCLK = 100 MHz (theo PLL config trong main.c)
     * Mỗi vòng lặp dưới đây ~ 4 chu kỳ -> 25 vòng = ~1μs
     */
    uint32_t loops = us * 25;
    while (loops--) {
        __NOP();
    }
}

/**
 *

 * @brief Khởi tạo chân LED (nếu chưa được cấu hình trong CubeMX)
 */



void GP2Y_Init(void)
{
    /* Đảm bảo LED tắt ban đầu (active LOW -> set HIGH) */
    HAL_GPIO_WritePin(GP2Y_LED_GPIO_Port, GP2Y_LED_Pin, GPIO_PIN_SET);
}

float g_gp2y_v_clean = 0.6f;   // mặc định, sẽ bị ghi đè khi hiệu chuẩn

/* Gọi 1 lần lúc khởi động, đặt cảm biến trong không khí sạch */
float GP2Y_CalibrateBaseline(ADC_HandleTypeDef *hadc, uint16_t n)
{
    float sum = 0.0f;
    for (uint16_t i = 0; i < n; i++) {
        HAL_GPIO_WritePin(GP2Y_LED_GPIO_Port, GP2Y_LED_Pin, GPIO_PIN_RESET);
        delay_us(280);
        uint16_t adc = MQ_ReadADC(hadc, GP2Y_ADC_CHANNEL);
        delay_us(40);
        HAL_GPIO_WritePin(GP2Y_LED_GPIO_Port, GP2Y_LED_Pin, GPIO_PIN_SET);
        sum += ((float)adc * GP2Y_ADC_VREF) / GP2Y_ADC_RESOLUTION;
        HAL_Delay(10);
    }
    g_gp2y_v_clean = sum / (float)n;
    return g_gp2y_v_clean;
}

/**
 * @brief Đọc cảm biến bụi theo đúng thời gian quy định trong datasheet
 */
void GP2Y_Read(ADC_HandleTypeDef *hadc, GP2Y_Data_t *data)
{
    /* 1. Bật LED (kéo LOW) */
    HAL_GPIO_WritePin(GP2Y_LED_GPIO_Port, GP2Y_LED_Pin, GPIO_PIN_RESET);

    /* 2. Chờ 280 μs để LED ổn định */
    delay_us(280);

    /* 3. Đọc ADC chân Vo */
    data->raw_adc = MQ_ReadADC(hadc, GP2Y_ADC_CHANNEL);

    /* 4. Chờ 40 μs */
    delay_us(40);

    /* 5. Tắt LED */
    HAL_GPIO_WritePin(GP2Y_LED_GPIO_Port, GP2Y_LED_Pin, GPIO_PIN_SET);

    /* 6. Chu kỳ tối thiểu 10 ms - phần còn lại sẽ do task delay */

    /* 7. Chuyển đổi sang điện áp & mật độ bụi */
    // 0.005 V/(µg/m³)
    /* Điện áp đo qua bộ chia 11k/(11k+22k) ~ 1/3 nếu dùng module 5V chia áp.
     * Nếu cấp 3.3V trực tiếp thì không cần nhân hệ số. Mặc định ở đây
     * giả sử module đã chia áp về mức an toàn cho STM32, hoặc cảm biến cấp 5V
     * và Vo chia áp về <3.3V. Hiệu chỉnh hệ số `gain` cho phù hợp module thực tế.
     */
    /* Bụi tỉ lệ với phần điện áp VƯỢT trên mức nền không-bụi đã hiệu chuẩn */
    data->voltage = ((float)data->raw_adc * GP2Y_ADC_VREF) / GP2Y_ADC_RESOLUTION;
    float delta = (data->voltage - g_gp2y_v_clean) * GP2Y_GAIN;
        if (delta < 0.0f) delta = 0.0f;
        data->dust_ug_m3 = delta / GP2Y_SENSITIVITY;
        data->dust_mg_m3 = data->dust_ug_m3 / 1000.0f;

    /* 8. Đánh giá chất lượng không khí theo PM2.5 (μg/m³) - chuẩn WHO/US-EPA:
     *    0   - 12    : Tốt
     *    12  - 35.4  : Trung bình
     *    35.4- 55.4  : Kém (nhạy cảm)
     *    55.4- 150.4 : Xấu
     *    > 150.4     : Nguy hiểm
     */
    if      (data->dust_ug_m3 <  35.0f)  data->air_quality_level = 1;
    else if (data->dust_ug_m3 <  80.0f)  data->air_quality_level = 2;
    else if (data->dust_ug_m3 <  120.4f)  data->air_quality_level = 3;
    else if (data->dust_ug_m3 < 250.4f)  data->air_quality_level = 4;
    else                                 data->air_quality_level = 5;
}

const char* GP2Y_GetQualityString(uint8_t level)
{
    switch (level) {
        case 1:  return "TOT";
        case 2:  return "TB ";
        case 3:  return "KEM";
        case 4:  return "XAU";
        case 5:  return "NGUY HIEM";
        default: return "?  ";
    }
}
