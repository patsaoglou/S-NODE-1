#include "poster.h"

/* 
    Way to get response taken from thread: https://esp32.com/viewtopic.php?t=18930
 */
esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt){
    static char *output_buffer;  
    static int output_len;       

    switch (evt->event_id){
    case HTTP_EVENT_ON_DATA:
       
        if (!esp_http_client_is_chunked_response(evt->client)) {
            if (evt->user_data) {
                memcpy(evt->user_data + output_len, evt->data, evt->data_len);
            } else {
                if (output_buffer == NULL) {
                    output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                    output_len = 0;
                    if (output_buffer == NULL) {
                        ESP_LOGE("TAG", "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
            }
            output_len += evt->data_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD("TAG", "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    default:
        break;
    }
    return ESP_OK;
}

void handle_measurement_post(initializer_struct *config, measurements_handler_struct *measurement_handler){
    char reponse[JSON_RESPONSE_SIZE];
    int tries = 0;
    esp_http_client_config_t http_config = {0};
    cJSON *post_json, *response_json;
    char *post_data;

    http_config.url = config->server_config.api_post_ep;
    http_config.method = HTTP_METHOD_POST;
    http_config.user_data = reponse;
    http_config.event_handler = client_event_get_handler;

    esp_http_client_handle_t client = esp_http_client_init(&http_config);

    esp_http_client_set_header(client, "Content-Type", "application/json");

    post_json = cJSON_CreateObject();
    cJSON_AddStringToObject(post_json, "device_key", config->server_config.api_key);
    cJSON_AddNumberToObject(post_json, "temp", measurement_handler->measurement.temp);
    cJSON_AddNumberToObject(post_json, "humid", measurement_handler->measurement.humid);
    cJSON_AddNumberToObject(post_json, "pressure", measurement_handler->measurement.pressure);
    cJSON_AddNumberToObject(post_json, "battery_voltage", measurement_handler->measurement.battery_voltage);
    cJSON_AddNumberToObject(post_json, "solar_voltage", measurement_handler->measurement.solar_voltage);
    cJSON_AddNumberToObject(post_json, "solar_current",  measurement_handler->measurement.solar_current);
    cJSON_AddNumberToObject(post_json, "wifi_signal_strength",  measurement_handler->measurement.wifi_signal_strength);

    post_data = cJSON_Print(post_json);

    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);
    
    while (tries < MAX_POST_TRIES && err != ESP_OK){
        err = esp_http_client_perform(client);
        tries ++;
    }

    if (err == ESP_OK) {
        ESP_LOGI("handle_measurement_post", "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        
        response_json = cJSON_Parse(reponse);
        response_json = cJSON_GetObjectItem(response_json, "sleep_duration");      
        ESP_LOGI("handle_measurement_post", "sleep_duration response: %d", response_json->valueint);

        if (cJSON_IsNumber(response_json)){
            save_sleep_duration_if_different(config, response_json->valueint);

            // indicate good cicle
            gpio_set_level(SETUP_LED,1);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
                
    } else {
        error_blinker(HTTP_POST_ERR);
        ESP_LOGE("handle_measurement_post", "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(post_json); 

    free(post_data);
}

void save_sleep_duration_if_different(initializer_struct *config, int duration_response){
    if ( duration_response> 0 && config->sleep_duration != duration_response){
        config->sleep_duration = duration_response;
        save_configuration(config);
    }
}


