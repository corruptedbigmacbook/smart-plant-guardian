#pragma once

typedef struct {
	char mac_address[18];
    float  temperature;
    float  pressure;
    float  humidity;
    float soil_moisture;
    float lux;
} sensor_data_t;
