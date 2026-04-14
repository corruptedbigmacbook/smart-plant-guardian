#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "mqtt_client.h"

#include "common/app_events.h"
#include "net_wifi.h"
#include "net_mqtt.h"



static const char *TAG = "NET_MQTT";

#define MQTT_BROKER_URI "mqtt://test.mosquitto.org"

static esp_mqtt_client_handle_t s_client = NULL;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "MQTT connected");
        xEventGroupSetBits(net_event_group(), MQTT_CONNECTED_BIT);
    }
    else if (event_id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "MQTT disconnected");
        xEventGroupClearBits(net_event_group(), MQTT_CONNECTED_BIT);
    }
}




static void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {0};
 cfg.broker.address.uri = MQTT_BROKER_URI;


s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}




void mqtt_start(void)
{
    // Gate: don't start MQTT until Wi-Fi is ready
    xEventGroupWaitBits(
        net_event_group(),
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );

    ESP_LOGI(TAG, "Wi-Fi ready -> starting MQTT once...");
    mqtt_init();
}


int mqtt_publish(const char *topic, const char *payload)
{
    // Safety gate: only publish when MQTT is connected
    EventBits_t bits = xEventGroupGetBits(net_event_group());
    if (!(bits & MQTT_CONNECTED_BIT)) {
        return -1; // not connected
    }

    if (s_client == NULL) {
        return -2; // should not happen if start is correct
    }

    (void)esp_mqtt_client_publish(
        s_client,
        topic,
        payload,
        0,  // auto length
        0,  // QoS 0
        0   // retain false
    );

    return 0;
}
