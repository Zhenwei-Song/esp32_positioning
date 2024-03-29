#ifndef _ALL_TASKS_H
#define _ALL_TASKS_H

#include "./main.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef USING_DMP
void ins_init(void);
void gpio_task(void *arg);
#endif

#ifdef USING_RAW
void timer3_check_task(void *pvParameters);
#endif //USING_RAW

#ifdef ONLY_ATT
extern SemaphoreHandle_t xCountingSemaphore_data_update;
void data_update(void *pvParameters);
#endif // ONLY_ATT

#ifdef PSINS_POS
extern SemaphoreHandle_t xCountingSemaphore_data_update_psins_pos;
void data_update_psins_pos(void *pvParameters);
#endif // PSINS_POS

#ifdef USING_SFANN_SINS
extern SemaphoreHandle_t xCountingSemaphore_data_update_sins_pos;
void data_update_sins_pos(void *pvParameters);
#endif // USING_SFANN

#ifdef USING_SPI
void timer1_check_task(void *pvParameters);
#endif // USING_SPI

#endif