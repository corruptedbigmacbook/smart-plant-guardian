#pragma once

void mqtt_start(void);
int mqtt_publish(const char *topic , const char *payload);
