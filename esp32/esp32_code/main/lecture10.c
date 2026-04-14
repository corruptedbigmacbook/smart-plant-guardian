#include <stdio.h>
#include <string.h>

#include "mqtt_client.h"
#define MQTT_BROKER_URI "mqtt://test.mosquitto.org"
#define MQTT_TOPIC     "esp32/sensor"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "freertos/queue.h"
#include "driver/uart.h"


#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "wifi_credentials.h"
#include "freertos/event_groups.h"
#define ADC_CHANNEL ADC_CHANNEL_7    
#define SAMPLE_MS 1000    
 #define ADC_UNIT ADC_UNIT_1
#define SAMPLES 32
#define MQTT_CONNECTED_BIT BIT1
static const char *TAG = "NET";

//wifi ready gate
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

//-----------------mqtt_event_handler-------------------
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{

    if (event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI("MQTT", "MQTT connected");
	xEventGroupSetBits(wifi_event_group,MQTT_CONNECTED_BIT);
    }

    if (event_id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGW("MQTT", "MQTT disconnected");
	xEventGroupClearBits(wifi_event_group,MQTT_CONNECTED_BIT);
    }
}

//---------------mqtt init-------------
static esp_mqtt_client_handle_t mqtt_client;

static void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );

    esp_mqtt_client_start(mqtt_client);
}

//-----------------wifi: event handler-----------------
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started -> connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected -> retrying...");
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP -> WiFi READY");
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

//------------------wifi initialisation --------------
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

//---------------------network task---------------------------------------
static void network_task(void *arg)
{
    wifi_init_sta();

 xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
   ESP_LOGI(TAG, "Network task: WiFi connected (gate open)."); 
      mqtt_init();
    // Keep alive (later we can monitor/reconnect metrics here)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


//----------------uart init-------------------------
static void uart0_init(void){
	uart_config_t cfg = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(UART_NUM_0, &cfg);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
             UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
             uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0);

}
static QueueHandle_t sensor_queue = NULL;
typedef struct {
    int raw;
    int mv;
} sensor_data_t;


//-----------------sensor structs-----------
void sensor_task(void *arg){
    adc_oneshot_unit_handle_t adc_handle;

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT,
        };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg,&adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
        };
        adc_oneshot_config_channel(adc_handle,ADC_CHANNEL,&chan_cfg);
        
        TickType_t last_wake = xTaskGetTickCount();
        while(1){
        int sum=0;
        for(int i =0; i<SAMPLES ; i++){
            int raw;
            ESP_ERROR_CHECK(adc_oneshot_read(adc_handle,ADC_CHANNEL,&raw));
            sum+= raw;
            
            }

        float averaged_raw =(float) sum/32;
        
        float calculated_mv = (averaged_raw*3300.0f) /4095.0f;
        
        sensor_data_t data;
        data.raw =(int) averaged_raw;
        data.mv = (int)calculated_mv;
        
        xQueueSend(sensor_queue, &data, portMAX_DELAY);
    
           xTaskDelayUntil(&last_wake,pdMS_TO_TICKS(SAMPLE_MS));   
        }
        }    
//-----------telemetry task----------
void telemetry_task(void *arg){
	//----------uart telemetry---------
uart0_init();
	sensor_data_t data;
	while(1){
	xQueueReceive(sensor_queue,&data,portMAX_DELAY);
    char line[64];
    int n = snprintf(line , sizeof(line), "{\"RAW\" :%d , \"MV\" :%d}\n", data.raw, data.mv);
   uart_write_bytes(UART_NUM_0, line ,n);
   //------------------mqtt telemetry 
  xEventGroupWaitBits(wifi_event_group, MQTT_CONNECTED_BIT,pdFALSE, pdTRUE,portMAX_DELAY);
   char payload[64];
snprintf(payload, sizeof(payload),
         "{\"RAW\" :%d , \"MV\" :%d}",
         data.raw, data.mv);
if (mqtt_client){
esp_mqtt_client_publish(
    mqtt_client,
    MQTT_TOPIC,
    payload,
    0,   // auto length
    0,   // QoS 0
    0    // no retain
);
}
}
}



//-----------------main function-------------------
	void app_main(void)
{

wifi_event_group = xEventGroupCreate();
xTaskCreate(network_task,"network_task",6144,NULL,5,NULL);
sensor_queue = xQueueCreate(4,sizeof(sensor_data_t));
xTaskCreate(sensor_task,"sensor_task",2048,NULL,5,NULL);
xTaskCreate(telemetry_task,"telemetry_task",2096,NULL,5,NULL);
}

