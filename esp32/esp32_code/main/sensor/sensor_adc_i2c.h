#pragma once
#include "freertos/queue.h"

void sensor_task_start(QueueHandle_t out_queue);
