#pragma once
#include "freertos/queue.h"

// Start telemetry task (it will receive sensor_data_t from this queue)
void telemetry_task_start(QueueHandle_t in_queue);
