/**
  ******************************************************************************
  * @file    air_quality.c
  * @brief   Triển khai logic trạm không khí với 3 task FreeRTOS
  ******************************************************************************
  */
#include "air_quality.h"
#include "st7735.h"
#include <stdio.h>
#include <string.h>

/* Tham chiếu các handle do CubeMX sinh ra */
extern ADC_HandleTypeDef  hadc1;
extern SPI_HandleTypeDef  hspi1;
extern UART_HandleTypeDef huart1;

/* Vùng dữ liệu dùng chung & mutex bảo vệ */
AirStation_Data_t g_air_data = {0};
osMutexId_t     dataMutexHandle;
osSemaphoreId_t initDoneSemHandle;

const osMutexAttr_t dataMutex_attr = { .name = "dataMutex" };

/* ============================================================================
 *  Khởi tạo
 * ==========================================================================*/
void AirStation_Init(void)
{
    /* Tạo mutex */
    dataMutexHandle = osMutexNew(&dataMutex_attr);

    /* Khởi tạo cảm biến bụi (LED ban đầu TẮT) */
    GP2Y_Init();

    /* Khởi tạo TFT */
    ST7735_Init(&hspi1);

    /* Màn hình khởi động */
    ST7735_FillScreen(ST7735_BLACK);
    ST7735_DrawString(8,   8, "AIR  STATION",   ST7735_CYAN,   ST7735_BLACK, 1);
    ST7735_DrawHLine(0, 20, 128, ST7735_CYAN);
    ST7735_DrawString(8,  30, "Initializing.. ", ST7735_WHITE,  ST7735_BLACK, 1);
    ST7735_DrawString(8,  50, "MQ2  MQ9  MQ135", ST7735_YELLOW, ST7735_BLACK, 1);
    ST7735_DrawString(8,  62, "+ Dust GP2Y10",   ST7735_YELLOW, ST7735_BLACK, 1);
    ST7735_DrawString(8,  90, "Warming up 20s",  ST7735_ORANGE, ST7735_BLACK, 1);

    /* MQ sensors cần ~20 giây gia nhiệt để ổn định
     * Dùng osDelay thay HAL_Delay để không block scheduler */
    osDelay(20000);
    /* Hiệu chuẩn nền trong không khí sạch */
        GP2Y_CalibrateBaseline(&hadc1, 30);
        g_mq2_r0   = MQ_Calibrate_R0(&hadc1, MQ2_ADC_CHANNEL,   9.83f, 50);
        g_mq9_r0   = MQ_Calibrate_R0(&hadc1, MQ9_ADC_CHANNEL,    9.6f, 50);
        g_mq135_r0 = MQ_Calibrate_R0(&hadc1, MQ135_ADC_CHANNEL,  3.6f, 50);
    /* Báo hiệu cho Task_DisplayUpdate và Task_UART_Report biết init xong */
    initDoneSemHandle = osSemaphoreNew(2, 0, NULL);
    osSemaphoreRelease(initDoneSemHandle);  // release lần 1 cho Task_DisplayUpdate
    osSemaphoreRelease(initDoneSemHandle);  // release lần 2 cho Task_UART_Report

    /* Thông báo qua UART */
    const char *msg = "\r\n[AIR STATION] System ready.\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
}

/* ============================================================================
 *  Task 1: Đọc cảm biến (10 Hz cho bụi, 1 Hz cho MQ)
 * ==========================================================================*/
void Task_SensorRead(void *argument)
{
    AirStation_Init();

    uint8_t mq_counter = 0;

    for (;;) {
        GP2Y_Data_t  dust_tmp;
        MQ2_Data_t   mq2_tmp   = {0};
        MQ9_Data_t   mq9_tmp   = {0};
        MQ135_Data_t mq135_tmp = {0};

        /* Đọc bụi mỗi 100ms */
        GP2Y_Read(&hadc1, &dust_tmp);

        /* Đọc MQ mỗi 1 giây (10 lần đọc bụi) */
        uint8_t mq_updated = 0;
        if (++mq_counter >= 10) {
            mq_counter = 0;
            MQ2_Read(&hadc1,   &mq2_tmp);
            MQ9_Read(&hadc1,   &mq9_tmp);
            MQ135_Read(&hadc1, &mq135_tmp);
            mq_updated = 1;
        }

        /* Cập nhật vào vùng dữ liệu chung */
        if (osMutexAcquire(dataMutexHandle, 50) == osOK) {
            g_air_data.dust = dust_tmp;
            if (mq_updated) {
                g_air_data.mq2   = mq2_tmp;
                g_air_data.mq9   = mq9_tmp;
                g_air_data.mq135 = mq135_tmp;
            }
            g_air_data.sample_count++;
            osMutexRelease(dataMutexHandle);
        }

        osDelay(100);   // 10 Hz
    }
}

/* ============================================================================
 *  Task 2: Cập nhật màn hình TFT (2 Hz)
 * ==========================================================================*/
static uint16_t aqi_color(uint8_t level)
{
    switch (level) {
        case 1:  return ST7735_GREEN;
        case 2:  return ST7735_YELLOW;
        case 3:  return ST7735_ORANGE;
        case 4:  return ST7735_RED;
        case 5:  return ST7735_MAGENTA;
        default: return ST7735_WHITE;
    }
}
//zzzz
/* Đánh giá 1 chỉ số theo 4 ngưỡng, trả về mức 1-5 */
static uint8_t eval_level(float val, float t1, float t2, float t3, float t4)
{
    if (val < t1) return 1;
    if (val < t2) return 2;
    if (val < t3) return 3;
    if (val < t4) return 4;
    return 5;
}

/* Lấy mức XẤU NHẤT trong tất cả cảm biến */
static uint8_t AirStation_OverallLevel(AirStation_Data_t *d)
{
    uint8_t worst = d->dust.air_quality_level;  // bụi đã tính sẵn

    uint8_t lv;

    /* CO (ppm) — QCVN/WHO 8h: 9 pp */
    lv = eval_level(d->mq9.ppm_co, 80.0f, 220.0f, 380.0f, 500.0f);
    if (lv > worst) worst = lv;

    /* CH4 (ppm) */
    lv = eval_level(d->mq9.ppm_ch4, 50.0f, 400.0f, 1500.0f, 2500.0f);
    if (lv > worst) worst = lv;

    /* LPG (ppm) */
    lv = eval_level(d->mq2.ppm_lpg, 50.0f, 400.0f, 1500.0f, 2500.0f);
    if (lv > worst) worst = lv;

    /* Khói (ppm) */
    lv = eval_level(d->mq2.ppm_smoke, 50.0f, 400.0f, 1500.0f, 2500.0f);
    if (lv > worst) worst = lv;

    /* H2 (ppm) */
    lv = eval_level(d->mq2.ppm_h2, 50.0f, 400.0f, 1500.0f, 2500.0f);
    if (lv > worst) worst = lv;

    /* CO2 (ppm) — trong nhà: <1000 OK */
    lv = eval_level(d->mq135.ppm_co2, 50.0f, 400.0f, 1500.0f, 2500.0f);
    if (lv > worst) worst = lv;

    /* NH3 (ppm) */
    lv = eval_level(d->mq135.ppm_nh3, 25.0f, 50.0f, 100.0f, 200.0f);
    if (lv > worst) worst = lv;

    return worst;
}

////zzzzz


void Task_DisplayUpdate(void *argument)
{
    /* Chờ AirStation_Init() hoàn tất thay vì osDelay cứng */
    osSemaphoreAcquire(initDoneSemHandle, osWaitForever);

    ST7735_FillScreen(ST7735_BLACK);
    ST7735_DrawString(20, 2, "AIR STATION", ST7735_CYAN, ST7735_BLACK, 1);
    ST7735_DrawHLine(0, 12, 128, ST7735_CYAN);

    /* Nhãn cố định */
    ST7735_DrawString(2,  16, "Dust:",    ST7735_WHITE, ST7735_BLACK, 1);
    ST7735_DrawString(2,  32, "CO  :",    ST7735_WHITE, ST7735_BLACK, 1);
    ST7735_DrawString(2,  44, "LPG :",    ST7735_WHITE, ST7735_BLACK, 1);
    ST7735_DrawString(2,  56, "Smk :",    ST7735_WHITE, ST7735_BLACK, 1);
    ST7735_DrawString(2,  68, "CO2 :",    ST7735_WHITE, ST7735_BLACK, 1);
    ST7735_DrawString(2,  80, "NH3 :",    ST7735_WHITE, ST7735_BLACK, 1);
    ST7735_DrawHLine(0, 95, 128, ST7735_CYAN);
    ST7735_DrawString(2, 100, "Quality:", ST7735_WHITE, ST7735_BLACK, 1);

    char buf[32];
    AirStation_Data_t snapshot;

    for (;;) {
        /* Chụp dữ liệu nhanh để render không khoá mutex lâu */
        if (osMutexAcquire(dataMutexHandle, 100) == osOK) {
            snapshot = g_air_data;
            osMutexRelease(dataMutexHandle);
        } else {
            osDelay(500);
            continue;
        }

        /* Bụi (μg/m³) */
        snprintf(buf, sizeof(buf), "%5.1f ug/m3   ", snapshot.dust.dust_ug_m3);
        ST7735_DrawString(38, 16, buf, aqi_color(snapshot.dust.air_quality_level), ST7735_BLACK, 1);

        /* MQ-9 CO */
        snprintf(buf, sizeof(buf), "%6.1f ppm   ", snapshot.mq9.ppm_co);
        ST7735_DrawString(38, 32, buf, ST7735_YELLOW, ST7735_BLACK, 1);

        /* MQ-2 LPG */
        snprintf(buf, sizeof(buf), "%6.1f ppm   ", snapshot.mq2.ppm_lpg);
        ST7735_DrawString(38, 44, buf, ST7735_YELLOW, ST7735_BLACK, 1);

        /* MQ-2 Smoke */
        snprintf(buf, sizeof(buf), "%6.1f ppm   ", snapshot.mq2.ppm_smoke);
        ST7735_DrawString(38, 56, buf, ST7735_YELLOW, ST7735_BLACK, 1);

        /* MQ-135 CO2 */
        snprintf(buf, sizeof(buf), "%6.1f ppm   ", snapshot.mq135.ppm_co2);
        ST7735_DrawString(38, 68, buf, ST7735_YELLOW, ST7735_BLACK, 1);

        /* MQ-135 NH3 */
        snprintf(buf, sizeof(buf), "%6.1f ppm   ", snapshot.mq135.ppm_nh3);
        ST7735_DrawString(38, 80, buf, ST7735_YELLOW, ST7735_BLACK, 1);

        /* Trạng thái tổng thể (dựa vào mức bụi) */
        uint8_t overall = AirStation_OverallLevel(&snapshot);
                snprintf(buf, sizeof(buf), "%-9s", GP2Y_GetQualityString(overall));
                ST7735_DrawString(60, 100, buf, aqi_color(overall), ST7735_BLACK, 1);

                /* --- Vùng ghi chú (y=116..148, tối đa 3 dòng) --- */
                /* Xóa 3 dòng cũ */
                ST7735_DrawString(2, 116, "                     ", ST7735_BLACK, ST7735_BLACK, 1);
                ST7735_DrawString(2, 128, "                     ", ST7735_BLACK, ST7735_BLACK, 1);
                ST7735_DrawString(2, 140, "                     ", ST7735_BLACK, ST7735_BLACK, 1);

                uint8_t ny = 116, nc = 0;

                if ((snapshot.mq2.ppm_lpg > 400.0f || snapshot.mq9.ppm_ch4 > 400.0f) && nc < 3) {
                                    ST7735_DrawString(2, ny, ">Kiem tra gas ngay! ", ST7735_RED, ST7735_BLACK, 1);
                                    ny += 12; nc++;
                                }
                                if (snapshot.mq9.ppm_co > 220.0f && nc < 3) {
                                    ST7735_DrawString(2, ny, ">Mo cua thoang khi! ", ST7735_YELLOW, ST7735_BLACK, 1);
                                    ny += 12; nc++;
                                }
                                if (snapshot.mq2.ppm_smoke > 400.0f && nc < 3) {
                                    ST7735_DrawString(2, ny, ">Phat hien khoi!    ", ST7735_RED, ST7735_BLACK, 1);
                                    ny += 12; nc++;
                                }
                                if (snapshot.dust.dust_ug_m3 > 80.0f && nc < 3) {
                                    ST7735_DrawString(2, ny, ">Bat may loc kk     ", ST7735_YELLOW, ST7735_BLACK, 1);
                                    ny += 12; nc++;
                                }
                                if (snapshot.mq135.ppm_co2 > 400.0f && nc < 3) {
                                    ST7735_DrawString(2, ny, ">Mo cua so          ", ST7735_YELLOW, ST7735_BLACK, 1);
                                    ny += 12; nc++;
                                }
                                if (snapshot.mq135.ppm_nh3 > 50.0f && nc < 3) {
                                    ST7735_DrawString(2, ny, ">Kiem tra khu vuc   ", ST7735_YELLOW, ST7735_BLACK, 1);
                                    ny += 12; nc++;
                                }
                osDelay(500);   // 2 Hz
                ST7735_DrawHLine(0, 112, 128, ST7735_CYAN);

    }
}

/* ============================================================================
 *  Task 3: Báo cáo qua UART (mỗi 2 giây, định dạng CSV)
 * ==========================================================================*/
void Task_UART_Report(void *argument)
{
    /* Chờ AirStation_Init() hoàn tất thay vì osDelay cứng */
    osSemaphoreAcquire(initDoneSemHandle, osWaitForever);

    char line[160];
    const char *header =
           "\r\nt(s),dust_ug,CO,LPG,Smoke,H2,CH4,CO2,NH3,Alcohol,AQI,Level\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)header, strlen(header), 100);

    uint32_t t = 0;
    AirStation_Data_t snap;

    for (;;) {
        if (osMutexAcquire(dataMutexHandle, 100) == osOK) {
            snap = g_air_data;
            osMutexRelease(dataMutexHandle);

            uint8_t overall = AirStation_OverallLevel(&snap);
                        int n = snprintf(line, sizeof(line),
                            "%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%u\r\n",
                            (unsigned long)t,
                            snap.dust.dust_ug_m3,
                            snap.mq9.ppm_co,
                            snap.mq2.ppm_lpg,
                            snap.mq2.ppm_smoke,
                            snap.mq2.ppm_h2,
                            snap.mq9.ppm_ch4,
                            snap.mq135.ppm_co2,
                            snap.mq135.ppm_nh3,
                            snap.mq135.ppm_alcohol,
                            snap.mq135.aqi_estimate,
                            overall
                        );
            if (n > 0) {
                HAL_UART_Transmit(&huart1, (uint8_t*)line, n, 200);
            }
        }
        t += 2;
        osDelay(2000);
    }
}
