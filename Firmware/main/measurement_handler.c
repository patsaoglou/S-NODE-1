#include "measurement_handler.h"

void init_adc(measurements_handler_struct *measurements_handler){ 
    adc_cali_curve_fitting_config_t cali_config = {
    .unit_id = ADC_UNIT_1,
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &measurements_handler->cali_handle));

    adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &measurements_handler->handle));

    adc_oneshot_chan_cfg_t config = {

    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(measurements_handler->handle, ADC_V_BAT, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(measurements_handler->handle, ADC_V_SOL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(measurements_handler->handle, ADC_SOL_CSENSE, &config));

} 

void init_sensor_i2c(measurements_handler_struct *measurements_handler){
    
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_18,
        .scl_io_num = GPIO_NUM_19,
        .sda_pullup_en = false,
        .scl_pullup_en = false,

        .master = {
            .clk_speed = 100000
        }
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    measurements_handler->bmx280 = bmx280_create(I2C_NUM_0);

    if (!measurements_handler->bmx280) { 
        ESP_LOGE("init_sensor_i2c", "Could not create bmx280 driver.");
    }

    ESP_ERROR_CHECK(bmx280_init(measurements_handler->bmx280));

    bmx280_config_t bmx_cfg = BMX280_DEFAULT_CONFIG;
    ESP_ERROR_CHECK(bmx280_configure(measurements_handler->bmx280, &bmx_cfg));

}

void init_measurement_handler(measurements_handler_struct *measurements_handler){
    init_sensor_i2c(measurements_handler);
    init_adc(measurements_handler);
}

void measure_all(measurements_handler_struct *measurements_handler){
    read_bme280_measurements(measurements_handler);
    read_battery_voltage(measurements_handler);
    read_solar_voltage(measurements_handler);
    read_solar_current(measurements_handler);
    read_wifi_signal_strength(measurements_handler);
}

void read_bme280_measurements(measurements_handler_struct *measurements_handler){
    ESP_ERROR_CHECK(bmx280_setMode(measurements_handler->bmx280, BMX280_MODE_FORCE));

    do {
        vTaskDelay(pdMS_TO_TICKS(1));
    } while(bmx280_isSampling(measurements_handler->bmx280));

    esp_err_t ret = bmx280_readoutFloat(measurements_handler->bmx280, &measurements_handler->measurement.temp, &measurements_handler->measurement.pressure, &measurements_handler->measurement.humid);
    if (ret != ESP_OK) {
        ESP_LOGE("SENSOR", "Failed to read sensor data");
    }
}

void read_battery_voltage(measurements_handler_struct *measurements_handler){
    int adc_value;

    adc_oneshot_read(measurements_handler->handle, ADC_V_BAT, &adc_value);
    
    adc_cali_raw_to_voltage(measurements_handler->cali_handle, adc_value, &adc_value);
    
    measurements_handler->measurement.battery_voltage = adc_value / 1000.0; 
}

void read_solar_voltage(measurements_handler_struct *measurements_handler){
    int adc_value;

    adc_oneshot_read(measurements_handler->handle, ADC_V_SOL, &adc_value);
    adc_cali_raw_to_voltage(measurements_handler->cali_handle, adc_value, &adc_value);
    
    measurements_handler->measurement.solar_voltage= adc_value / 1000.0;
}

void read_solar_current(measurements_handler_struct *measurements_handler){
    int adc_value;

    adc_oneshot_read(measurements_handler->handle, ADC_SOL_CSENSE, &adc_value);
    adc_cali_raw_to_voltage(measurements_handler->cali_handle, adc_value, &adc_value);
    measurements_handler->measurement.solar_current = adc_value / CURRENT_SHUNT_FORM; 
    
}

void read_wifi_signal_strength(measurements_handler_struct *measurements_handler){
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);
    measurements_handler->measurement.wifi_signal_strength = (float) ap.rssi;
    
}