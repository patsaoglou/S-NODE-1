#ifndef _MAIN_H
#define _MAIN_H

#include "node_initializer.h"
#include "measurement_handler.h"
#include "poster.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

// setup button config to enter node into configuration mode
#define DEBOUNCE_DELAY 50 
#define TIME_FRAME 5000 
#define PRESS_COUNT_THRESHOLD 3

static QueueHandle_t gpio_evt_queue = NULL; 
uint8_t press_times = 0;
volatile bool in_config_mode = false;

void reset_task();
void setup_gpio();
void init_nvs();
void init_tasks();

#endif