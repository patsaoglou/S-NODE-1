#ifndef MEASUREMENT_HANDLER_H
#define MEASUREMENT_HANDLER_H
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "bmx280.h"

#define ADC_V_BAT ADC_CHANNEL_1
#define ADC_V_SOL ADC_CHANNEL_2
#define ADC_SOL_CSENSE ADC_CHANNEL_3

/* 

    For solar current readings a low side current shunt in implemented between
    the board's ground and the solar ground terminal.

    Selected shunt resistor used: R_shunt = 0.5 Omhs.

    Shunt voltage is fed to an Op Amp configured in x20 gain and the output is 
    fed to the ESP32 ADC pin.

    Formula: I_mA = V_shunt_mV / R_shunt = (ADC_reading_mV / x20 Gain) / 0.5 = ADC_reading_mV / 10.

    The read_solar_current function saves the current reading in mA.

 */
#define CURRENT_SHUNT_FORM 10.0

typedef struct 
{
    float temp;
    float humid;
    float pressure;
    float battery_voltage;
    float solar_voltage;
    float solar_current;
    float wifi_signal_strength;
}measurements;


typedef struct 
{
    adc_oneshot_unit_handle_t handle;
    adc_cali_handle_t cali_handle;
    bmx280_t* bmx280;

    measurements measurement; 
}measurements_handler_struct;


void init_adc(measurements_handler_struct *measurements_handler);
void init_sensor_i2c(measurements_handler_struct *measurements_handler);
void init_measurement_handler(measurements_handler_struct *measurements_handler);
void measure_all(measurements_handler_struct *measurements_handler);
void read_bme280_measurements(measurements_handler_struct *measurements_handler);
void read_battery_voltage(measurements_handler_struct *measurements_handler);
void read_solar_voltage(measurements_handler_struct *measurements_handler);
void read_solar_current(measurements_handler_struct *measurements_handler);
void read_wifi_signal_strength(measurements_handler_struct *measurements_handler);

#endif 
