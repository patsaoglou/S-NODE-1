#ifndef _NODE_INITIALIZER_H_
#define _NODE_INITIALIZER_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "esp_wifi_types.h"
#include "cJSON.h"
#include "html_src.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/gptimer.h"

#define NODE_AP_SSID            "S-NODE-1"
#define NODE_AP_PW              "SOLARNODE1"

#define JSON_RESPONSE_SIZE      1024
#define INITIALIZER_AP_UP_TIME  5
#define DEFAULT_SLEEP_DURATION  15

#define MAX_CONNECT_TRIES       2

#define SETUP_LED  GPIO_NUM_11
#define SETUP_GPIO GPIO_NUM_9  

#define API_FIELD_SIZE  128
#define SSID_SIZE       32
#define PW_SIZE         64

extern char *html_form;

typedef uint8_t config_state;

typedef struct 
{
    char api_post_ep[API_FIELD_SIZE];
    char api_key[API_FIELD_SIZE];
    uint8_t server_type;
}server_configuration;

typedef struct 
{
    char ap_ssid[SSID_SIZE];
    char ap_password[PW_SIZE];
}wifi_conn_config;

typedef struct 
{
    wifi_conn_config wifi_config;
    server_configuration server_config;

    uint8_t initializer_status;
    int64_t sleep_duration; 
}initializer_struct;

typedef enum
{
    INITIALIZER_BEGIN,
    CONFIG_AVAILABLE,
    CONFIG_UNAVAILABLE,
    AP_CONNECT_FAIL,
    AP_CONNECT_SUCCESS,
    INITIALIZER_OK,
    AP_TIME_ELAPSED,
    CONNECT_TRIGGERED,
}initializer_status_codes;

typedef enum{
    CYCLE_OK = 1,
    ENTERED_CONFIG_MODE = 3,
    WIFI_CRED_ERR = 4,
    HTTP_POST_ERR = 5,    
    WIFI_CRED_IN_CONFIG_ERR = 6,
    AP_TIMEOUT = 10
}led_error_blinks;

void init_node_config(initializer_struct *initializer);

void init_netif();
void reset_netif();
void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data);
void initialize_node_as_ap(initializer_struct *initializer);
void initialize_node_ap_server(initializer_struct *initializer);
config_state connect_to_ap(initializer_struct *initializer);

esp_err_t get_credenctials_and_connect(httpd_req_t *req);
esp_err_t get_node_config_page(httpd_req_t *req);
esp_err_t scan_for_local_networks(httpd_req_t *req);


esp_err_t save_configuration(initializer_struct *initializer);
config_state load_configuration(initializer_struct *initializer);
void delete_nvs_config();

void deep_sleep_for_sleep_duration(initializer_struct *initializer);

void error_blinker(u_int8_t err);
void init_ap_timer();

#endif
