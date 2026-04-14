#pragma once
#include "freertos/event_groups.h"

void wifi_start(void);
EventGroupHandle_t net_event_group(void);
