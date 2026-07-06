/**
  ******************************************************************************
  * @file    air_quality.h
  * @brief   Module logic ứng dụng trạm quan trắc không khí
  ******************************************************************************
  */
#ifndef AIR_QUALITY_H
#define AIR_QUALITY_H

#include "main.h"
#include "cmsis_os.h"
#include "mq_sensors.h"
#include "gp2y1010.h"

/* Cấu trúc tổng hợp dữ liệu toàn hệ thống */
typedef struct {
    MQ2_Data_t    mq2;
    MQ9_Data_t    mq9;
    MQ135_Data_t  mq135;
    GP2Y_Data_t   dust;
    uint32_t      sample_count;
} AirStation_Data_t;

/* Mutex bảo vệ vùng dữ liệu dùng chung */
extern osMutexId_t     dataMutexHandle;
extern osSemaphoreId_t initDoneSemHandle;
extern AirStation_Data_t g_air_data;

/* Khởi tạo toàn hệ thống (gọi từ Task_SensorRead) */
void AirStation_Init(void);

/* Các task FreeRTOS */
void Task_SensorRead(void *argument);
void Task_DisplayUpdate(void *argument);
void Task_UART_Report(void *argument);

#endif /* AIR_QUALITY_H */
