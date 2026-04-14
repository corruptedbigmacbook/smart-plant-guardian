#include "time_sync.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>


static const char *TAG = "time";



void time_sync_init(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

void time_sync_wait(void)
{
    time_t now = 0;
    struct tm timeinfo = {0};

    for (int i = 0; i < 20; i++) {
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year >= (2016 - 1900)) {
            ESP_LOGI(TAG, "Time synced.");
            return;
        }

        ESP_LOGI(TAG, "Waiting for NTP time...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGW(TAG, "NTP sync failed.");
}
