#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "common/app_types.h"

#include "net/net_wifi.h"
#include "net/net_mqtt.h"
#include "sensor/sensor_adc_i2c.h"
#include "telemetry/telemetry.h"

void app_main(void)
{
    QueueHandle_t q = xQueueCreate(4, sizeof(sensor_data_t));

    // Start network subsystems
    wifi_start();
    mqtt_start(); // internally waits for Wi-Fi ready

    // Start application tasks
    sensor_task_start(q);
    telemetry_task_start(q);
}

