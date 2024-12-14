#include "node_initializer.h"

initializer_struct *initializer;
static int retry_num = 0;

static httpd_handle_t server = NULL;
static esp_netif_t *net_interface = NULL;

void init_node_config(initializer_struct *initializer_p){ 
    initializer = initializer_p;
    initializer_p->initializer_status = INITIALIZER_BEGIN;
    initializer->sleep_duration = DEFAULT_SLEEP_DURATION;
    
    init_ap_timer();
    init_netif();

    if (load_configuration(initializer_p) == CONFIG_UNAVAILABLE){
        error_blinker(ENTERED_CONFIG_MODE);
        initialize_node_as_ap(initializer_p);
        initialize_node_ap_server(initializer_p);
        
        while(initializer_p->initializer_status != AP_CONNECT_SUCCESS && initializer_p->initializer_status != AP_TIME_ELAPSED){ 

            if(initializer_p->initializer_status == AP_CONNECT_FAIL){
                reset_netif();
                init_netif();
                initialize_node_as_ap(initializer_p);
                initialize_node_ap_server(initializer_p);
                error_blinker(WIFI_CRED_ERR);
                initializer_p->initializer_status = INITIALIZER_BEGIN;

            }else if (initializer_p->initializer_status == CONNECT_TRIGGERED){
                // connect request from config server is triggered
                
                httpd_stop(server);
                reset_netif();
                init_netif();
                connect_to_ap(initializer_p);
                initializer_p->initializer_status = INITIALIZER_BEGIN;

            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        if (initializer_p->initializer_status == AP_TIME_ELAPSED){
            ESP_LOGI("init_node_config", "AP time elapsed. Getting into deep sleep for DEFAULT_SLEEP_DURATION");

            error_blinker(AP_TIMEOUT);
            deep_sleep_for_sleep_duration(initializer);

        }
        save_configuration(initializer_p);
        
    }else{
        connect_to_ap(initializer_p);
        while (initializer_p->initializer_status != AP_CONNECT_SUCCESS && initializer_p->initializer_status != AP_CONNECT_FAIL){
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        
        // if for any reason existing wifi config fails 
        if (initializer_p->initializer_status == AP_CONNECT_FAIL){
            error_blinker(WIFI_CRED_IN_CONFIG_ERR);
            deep_sleep_for_sleep_duration(initializer);
        }
    }
}

/* 
    Switching from ap to sta and viceversa crashes solved using the thread: https://esp32.com/viewtopic.php?t=22486
    Also had an issue when code changes back to ap mode when fail to connect to ap after config connect: https://github.com/espressif/esp-idf/issues/4411

    Used esp_netif_destroy() because i had crash issues on re-init. Issue fixed by using esp_netif_destroy_default_wifi: https://github.com/espressif/esp-idf/issues/8702
 */
void reset_netif(){
    
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_deinit();

    // destroy must happen so netif create is successfull
    esp_netif_destroy_default_wifi(net_interface);
}

void init_netif(){
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
}

void initialize_node_as_ap(initializer_struct *initializer){
   
    net_interface = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = NODE_AP_SSID,
            .ssid_len = strlen(NODE_AP_SSID),
            .password = NODE_AP_PW,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void initialize_node_ap_server(initializer_struct *initializer){
    server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        
        httpd_uri_t get_node_config_page_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = get_node_config_page,
            .user_ctx = NULL
        };

        httpd_uri_t scan_for_local_network_uri = {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = scan_for_local_networks,
            .user_ctx = NULL
        };
        
        httpd_uri_t get_credenctials_and_connect_uri = {
            .uri = "/connect",
            .method = HTTP_POST,
            .handler = get_credenctials_and_connect,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &get_node_config_page_uri);
        httpd_register_uri_handler(server, &scan_for_local_network_uri);     
        httpd_register_uri_handler(server, &get_credenctials_and_connect_uri);     

    }
}

// request handler for node configuration server
esp_err_t get_node_config_page(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t scan_for_local_networks(httpd_req_t *req) {
    uint16_t n_ap = 0;
    wifi_ap_record_t ap_record;
    const char *ap_list_resp;
    cJSON *ap_list = cJSON_CreateArray();

    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = false
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&n_ap));


    for (; n_ap > 0; n_ap--) {
        cJSON *ap_obj = cJSON_CreateObject();
        esp_wifi_scan_get_ap_record(&ap_record);
        cJSON_AddStringToObject(ap_obj, "ssid", (char *)ap_record.ssid);
        cJSON_AddItemToArray(ap_list, ap_obj);
    }

    ap_list_resp = cJSON_Print(ap_list);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ap_list_resp, strlen(ap_list_resp));
    
    cJSON_Delete(ap_list);
    free((void *)ap_list_resp);

    return ESP_OK;

}

esp_err_t get_credenctials_and_connect(httpd_req_t *req){
    char buf[JSON_RESPONSE_SIZE];
    const cJSON *ssid_json, *password_json, *api_post_ep_json, *api_key_json;
    int ret = httpd_req_recv(req, buf, JSON_RESPONSE_SIZE);

    
    if (ret <= 0) {
        return ESP_FAIL;
    }
    
    buf[ret] = '\0';
    cJSON *json = cJSON_Parse(buf);
    
    if (json == NULL) {
        httpd_resp_send(req, "Null credentials JSON", HTTPD_RESP_USE_STRLEN);

        return ESP_FAIL;
    }

    ssid_json = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    password_json = cJSON_GetObjectItemCaseSensitive(json, "password");
    api_post_ep_json = cJSON_GetObjectItemCaseSensitive(json, "api_post_ep");
    api_key_json = cJSON_GetObjectItemCaseSensitive(json, "api_key");
        
    // Validate fields
    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(password_json) || 
            !cJSON_IsString(api_post_ep_json) || !cJSON_IsString(api_key_json)) {
        httpd_resp_send(req, "Invalid data type fields", HTTPD_RESP_USE_STRLEN);

        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Check string length and respond with length issue if user prompted something
    if (strlen(ssid_json->valuestring) > SSID_SIZE || strlen(password_json->valuestring) > PW_SIZE || 
            strlen(api_post_ep_json->valuestring) > API_FIELD_SIZE || strlen(api_key_json->valuestring) > API_FIELD_SIZE) {
        httpd_resp_send(req, "Field entered out of range. Max API related length 128/ SSID 32/ password 64", HTTPD_RESP_USE_STRLEN);

        cJSON_Delete(json);
        return ESP_FAIL;
    }

    strncpy(initializer->wifi_config.ap_ssid, ssid_json->valuestring, SSID_SIZE);
    strncpy(initializer->wifi_config.ap_password, password_json->valuestring, PW_SIZE);
    strncpy(initializer->server_config.api_post_ep, api_post_ep_json->valuestring, API_FIELD_SIZE);
    strncpy(initializer->server_config.api_key, api_key_json->valuestring, API_FIELD_SIZE);


    httpd_resp_send(req, "Connecting to Wi-Fi...", HTTPD_RESP_USE_STRLEN);
    initializer->initializer_status = CONNECT_TRIGGERED; 

    cJSON_Delete(json);
   
    return ESP_OK;
}

config_state connect_to_ap(initializer_struct *initializer){
    net_interface = esp_netif_create_default_wifi_sta();
   
    wifi_config_t wifi_configuration = {0};
    ESP_LOGI("connect_to_ap", "Setting up wifi sta config to wifi_start and connect.");
    
    //  This copy is within predefined size fields and ESP API structures is length safe here
    strcpy((char*)wifi_configuration.sta.ssid, initializer->wifi_config.ap_ssid);
    strcpy((char*)wifi_configuration.sta.password, initializer->wifi_config.ap_password);    
    
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_start();
    esp_wifi_connect();

    return ESP_OK;
}

esp_err_t save_configuration(initializer_struct *initializer){
    nvs_handle_t nvs_handler;
    ESP_LOGI("save_configuration ", "Saving configuration to NVS.");
    
    nvs_open("config", NVS_READWRITE, &nvs_handler);
    
    nvs_set_str(nvs_handler, "ssid", initializer->wifi_config.ap_ssid);
    nvs_set_str(nvs_handler, "password", initializer->wifi_config.ap_password);
    nvs_set_str(nvs_handler, "api_ep", initializer->server_config.api_post_ep);
    nvs_set_str(nvs_handler, "api_key", initializer->server_config.api_key);
    nvs_set_i64(nvs_handler, "sleep_duration", initializer->sleep_duration);

    nvs_commit(nvs_handler);
    nvs_close(nvs_handler);

    return 0;
}

config_state load_configuration(initializer_struct *initializer) {
    nvs_handle_t nvs_handler;
    size_t length = 0;
    esp_err_t err;

    nvs_open("config", NVS_READONLY, &nvs_handler);

    err = nvs_get_str(nvs_handler, "ssid", NULL, &length);  
   
    if (err == ESP_OK && length > 0) {
        nvs_get_str(nvs_handler, "ssid", initializer->wifi_config.ap_ssid, &length); 
        ESP_LOGI("load_configuration", "SSID found in NVS. Loading other config fields.");

    } else {
        ESP_LOGI("load_configuration", "SSID not found in NVS.");
        nvs_close(nvs_handler);
        return CONFIG_UNAVAILABLE;
    }
    
    if (length > 0){
        length = sizeof(initializer->wifi_config.ap_password);
        nvs_get_str(nvs_handler, "password", initializer->wifi_config.ap_password, &length);

        length = sizeof(initializer->server_config.api_post_ep);
        nvs_get_str(nvs_handler, "api_ep", initializer->server_config.api_post_ep, &length);

        length = sizeof(initializer->server_config.api_key);
        nvs_get_str(nvs_handler, "api_key", initializer->server_config.api_key, &length);

        nvs_get_i64(nvs_handler, "sleep_duration", &initializer->sleep_duration);
        nvs_close(nvs_handler);

        return CONFIG_AVAILABLE;
    }

    nvs_close(nvs_handler);

    return CONFIG_UNAVAILABLE;   
}

void delete_nvs_config() {
    nvs_handle_t nvs_handler;

    nvs_open("config", NVS_READWRITE, &nvs_handler);
    nvs_erase_all(nvs_handler);
    nvs_commit(nvs_handler);
    nvs_close(nvs_handler);
}

void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){
    if(event_id == WIFI_EVENT_STA_START){
        
        ESP_LOGI("wifi_event_handler", "WIFI CONNECTING...");

    }else if (event_id == WIFI_EVENT_STA_CONNECTED){
        
        ESP_LOGI("wifi_event_handler", "WiFi CONNECTED" );

    }else if (event_id == WIFI_EVENT_STA_DISCONNECTED){
        
        ESP_LOGI("wifi_event_handler", "WiFi lost connection");

        wifi_event_sta_disconnected_t *status = (wifi_event_sta_disconnected_t *)event_data;
        if (status->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT){
            ESP_LOGI("wifi_event_handler", "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT");

            goto finish_wifi_connect;
        }else if(retry_num < MAX_CONNECT_TRIES){
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI("wifi_event_handler", "Retrying to Connect to %s", initializer->wifi_config.ap_ssid);
            return;
        }
        finish_wifi_connect:
            retry_num = 0;
            initializer -> initializer_status = AP_CONNECT_FAIL;
            ESP_LOGI("wifi_event_handler", "finish_wifi_connect fail %s", initializer->wifi_config.ap_ssid);
        

    }else if (event_id == IP_EVENT_STA_GOT_IP){
        
        ESP_LOGI("wifi_event_handler", "Wifi got IP...");
        initializer -> initializer_status = AP_CONNECT_SUCCESS;
    }
}

void deep_sleep_for_sleep_duration(initializer_struct *initializer){
    esp_sleep_enable_timer_wakeup(initializer->sleep_duration * 60000000);
    esp_deep_sleep_start();
}

void error_blinker(uint8_t err) {    
    for (int blick_times = err;  blick_times> 0; blick_times--) {
        gpio_set_level(SETUP_LED, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        if (err != CYCLE_OK){
            gpio_set_level(SETUP_LED, 0);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
}

static bool IRAM_ATTR ap_timeout(){
    
    initializer->initializer_status = AP_TIME_ELAPSED;

    return true;
}

void init_ap_timer(){
    ESP_LOGI("setup_ap_timer", "Setting up AP timer");

    gptimer_handle_t gptimer = NULL;
    
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, 
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_alarm_config_t alarm = {
        .alarm_count = 60000000 * INITIALIZER_AP_UP_TIME,
        .flags.auto_reload_on_alarm = true 
    };
    
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm));

    gptimer_event_callbacks_t timer_callback_st = {
        .on_alarm = ap_timeout,
    };
     
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &timer_callback_st, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}
