
#ifndef POSTER_H
#define POSTER_H

#include "measurement_handler.h"
#include "node_initializer.h"
#include "esp_http_client.h"

#define MAX_POST_TRIES 5

void handle_measurement_post(initializer_struct *config, measurements_handler_struct *measurement_handler);
void save_sleep_duration_if_different(initializer_struct *config, int duration_response);

#endif