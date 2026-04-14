#include <stdio.h>
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "common/app_types.h"

#include "net/net_mqtt.h"
#include "telemetry.h"
#include <time.h>

   static const char *TAG = "TEL";
static QueueHandle_t s_in = NULL;

#define MQTT_TOPIC "esp32/sensor"   

void mac_address (char mac_str[18]){


uint8_t mac[6];
esp_read_mac(mac, ESP_MAC_WIFI_STA);

snprintf(mac_str, 18,
         "%02X:%02X:%02X:%02X:%02X:%02X",
         mac[0], mac[1], mac[2],
         mac[3], mac[4], mac[5]);

}

static void iso_time_now(char *buf, size_t len)
{
    time_t now;
    struct tm t;

    time(&now);
    gmtime_r(&now, &t);

    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &t);
}




static void telemetry_task(void *arg)
{
    sensor_data_t data;
	char mac_str[18];	 
	mac_address(mac_str);
    
    while (1) {
        xQueueReceive(s_in, &data, portMAX_DELAY);
	

    char iso[32];
    iso_time_now(iso,sizeof(iso));
        // Telemetry decides the payload format (policy / content)
        char payload[2000];
	snprintf(payload, sizeof(payload), "{\"timestamp\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,\"soil_moisture\":%.2f,\"light_lux\":%.2f,\"mac_address\":\"%s\"}", iso, data.temperature,data.humidity,data.soil_moisture,data.lux,mac_str);

        int rc = mqtt_publish(MQTT_TOPIC, payload);
        if (rc != 0) {
            // Telemetry policy decision: currently drop + log
            ESP_LOGW(TAG, "Publish skipped (rc=%d): %s", rc, payload);
        } else {
            ESP_LOGI(TAG, "Published: %s", payload);
        }
    }
}










void telemetry_task_start(QueueHandle_t in_queue)
{
    s_in = in_queue;
    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
}
