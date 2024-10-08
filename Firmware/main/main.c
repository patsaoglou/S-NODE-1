#include "main.h"

/*
    Main Concept of the DreamIOALabs S-NODE-1 Firmware:
    
    This firmware performs sensor measurements (humidity, temperature, etc.) and sends the data 
    to a server. The node can either operate in normal mode (measuring and sending data) or 
    enter configuration mode if specific conditions are met (config button pressed 3 times). 
    After completing its tasks, the node enters deep sleep until the next measurement cycle or
    if default hard coded time is elapse (see header files).
    
    1) GPIO Button Setup and Task Loop:
       - Initialize a GPIO button and create a task loop to monitor the button press.
       - If the button is pressed three times in succession, the node deletes the saved 
         configuration stored in NVS (Non-Volatile Storage).
       - Once the configuration is deleted, the node reboots with no config and enters config mode automatically, 
         allowing the user to provide new network and server settings.

    2) Network Configuration and AP Connection:
       - If a valid configuration is available in NVS, attempt to connect to the configured Access Point (AP).
       - Set a maximum number of connection attempts and a timeout. If the node fails to connect within 
         the limit, it will switch to configuration/AP mode to allow the user to re-enter network settings.
       - If the node cannot connect to the AP after the maximum number of tries, it enters deep sleep 
         for a specified duration (as defined by the last successful configuration stored in NVS).
    
    3) Deep Sleep on AP Connection Failure:
       - If the node fails to connect to the AP after the configured maximum number of attempts, 
         enter deep sleep for a duration previously saved in NVS.
       - The sleep duration is loaded from NVS through the `initializer_struct` during initialization.

    4) Sensor Initialization and Measurement:
       - Once connected to the AP, initialize the measurement handler. This involves setting up the 
         sensors (such as ADCs and the BME280 sensor).
       - Perform all sensor readings and store the values in the `measurements_handler.measurement` struct 
         to use later for post.

    5) Handling Measurements and Posting Data:
       - After performing all sensor measurements, execute `handle_measurement_post` to send the data 
         to a server endpoint specified during configuration.
       - The server's response will include the next sleep duration for the node. This duration controls 
         how long the node will sleep before the next measurement cycle.
       - If the new sleep duration differs from the one currently saved, the node updates the configuration 
         and saves the new sleep duration to NVS using the function `save_sleep_duration_if_different`.

    6) Cycle Completion and Deep Sleep:
       - After successfully posting the measurements to the server and handling the response, 
         the node completes its cycle and enters deep sleep.
       - The node will remain in deep sleep for the specified sleep duration (either from NVS or default) 
         until the next cycle is triggered.

    
    - Third-Party Library Licenses
    
    1)

      BMX280 - BME280 & BMP280 Driver for Esspressif ESP-32.
      
      MIT License
      
      Copyright (C) 2020 Halit Utku Maden
      Please contact at <utkumaden@hotmail.com>
    --------------------------------------------------------  
*/

void reset_task() {
    while (1) {
        if (in_config_mode == true){
            delete_nvs_config();
            esp_restart();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void IRAM_ATTR button_isr_handler() {
    static unsigned long last_press_time = 0;
    unsigned long time = esp_timer_get_time();

    if ((time - last_press_time) > DEBOUNCE_DELAY) {
        last_press_time = time;
        press_times++;

        if (time - last_press_time > TIME_FRAME) {
            press_times = 1; 
        }

        last_press_time = time;

        if (press_times >= PRESS_COUNT_THRESHOLD) {
            in_config_mode = true;
        }
    }
}

void setup_gpio(){
    in_config_mode = false;
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << SETUP_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(SETUP_GPIO, button_isr_handler, NULL);

    // gpio_sleep_output_enable(SETUP_LED);
    gpio_set_direction(SETUP_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(SETUP_LED, 0);

}

void init_nvs(){
esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void init_tasks(){
      gpio_evt_queue = xQueueCreate(10, sizeof(int));

    if (gpio_evt_queue == NULL) {
        ESP_LOGE("app_main", "Failed to create GPIO event queue!");
        return;
    }

    xTaskCreate(reset_task, "reset_task", 4096, NULL, 5, NULL);
}

void app_main(void) {
    initializer_struct initializer;
    measurements_handler_struct measurements_handler;

    setup_gpio();
    init_nvs();
    init_tasks();    

    init_node_config(&initializer);
    init_measurement_handler(&measurements_handler);

    measure_all(&measurements_handler);
    handle_measurement_post(&initializer, &measurements_handler);
    deep_sleep_for_sleep_duration(&initializer);
}

