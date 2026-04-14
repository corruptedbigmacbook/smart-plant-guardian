#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#include "common/app_types.h"

#include "sensor_adc_i2c.h"

#include <math.h>
#include <esp_log.h>
#include "driver/i2c_master.h"

#define ADC_UNIT     ADC_UNIT_1
#define ADC_CHANNEL  ADC_CHANNEL_7
#define SAMPLE_MS    1000
#define SAMPLES      32

static QueueHandle_t s_out = NULL;


static const char *TAG = "test";

#define I2C_MASTER_SCL_IO GPIO_NUM_33
#define I2C_MASTER_SDA_IO GPIO_NUM_32
#define I2C_MASTER_NUM  I2C_NUM_0 
#define I2C_MASTER_FREQ_HZ 100000

static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t TSL2561_handle  = NULL;
static i2c_master_dev_handle_t BME280_handle = NULL;
#define TSL2561_ADDR 0x39
#define BME280_ADDR 0x76
//-----------------unfinished personal BME280 driver-----------

//------------bme280 WRITE HELPER FUNCTION------
static esp_err_t bme_write8(uint8_t reg,uint8_t val){
	uint8_t buff[2] = {reg,val};
	return i2c_master_transmit(BME280_handle,buff,2,pdMS_TO_TICKS(100)
			);

}

//--------BME280 burst reading helper function----
static esp_err_t bme_readN(uint8_t start_reg , uint8_t *out,size_t n){
	
	return i2c_master_transmit_receive(
			BME280_handle,
			&start_reg,
			1,out,n,pdMS_TO_TICKS(100));

}

//-------compensation config functions and struct---
typedef struct {
    uint16_t dig_T1; int16_t dig_T2, dig_T3;
    uint16_t dig_P1; int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t  dig_H1; int16_t dig_H2; uint8_t dig_H3; int16_t dig_H4, dig_H5; int8_t dig_H6;
} bme280_calib_t;

static bme280_calib_t cal;
static int32_t t_fine;

static uint16_t u16_le(const uint8_t *b){ return (uint16_t)b[0] | ((uint16_t)b[1] << 8); }
static int16_t  s16_le(const uint8_t *b){ return (int16_t)u16_le(b); }

void cal_reg(void ){
    uint8_t c1[26], c2[7];
bme_readN(0x88, c1, 26);
bme_readN(0xE1, c2, 7);

cal.dig_T1 = u16_le(&c1[0]);
cal.dig_T2 = s16_le(&c1[2]);
cal.dig_T3 = s16_le(&c1[4]);

cal.dig_P1 = u16_le(&c1[6]);
cal.dig_P2 = s16_le(&c1[8]);
cal.dig_P3 = s16_le(&c1[10]);
cal.dig_P4 = s16_le(&c1[12]);
cal.dig_P5 = s16_le(&c1[14]);
cal.dig_P6 = s16_le(&c1[16]);
cal.dig_P7 = s16_le(&c1[18]);
cal.dig_P8 = s16_le(&c1[20]);
cal.dig_P9 = s16_le(&c1[22]);

cal.dig_H1 = c1[25];
cal.dig_H2 = s16_le(&c2[0]);
cal.dig_H3 = c2[2];
cal.dig_H4 = (int16_t)((c2[3] << 4) | (c2[4] & 0x0F));
cal.dig_H5 = (int16_t)((c2[5] << 4) | (c2[4] >> 4));
cal.dig_H6 = (int8_t)c2[6];

}

//-------BME280 driver CONFIG----
static esp_err_t bme_configure(void){
   
	//Humidity oversmapling cfg
	bme_write8(0xF2,0x02);
	//temp,pressure oversampling and mode cfg
	bme_write8(0xF4,0x4F);
	return ESP_OK;
}

//compensation formulas for bme280(temperature,pressure,humidty)
static float compensate_T(uint32_t adc_T)
{
    int32_t var1 = ((((int32_t)adc_T >> 3) - ((int32_t)cal.dig_T1 << 1)) *
                    ((int32_t)cal.dig_T2)) >> 11;

    int32_t var2 = (((((int32_t)adc_T >> 4) - ((int32_t)cal.dig_T1)) *
                    (((int32_t)adc_T >> 4) - ((int32_t)cal.dig_T1))) >> 12) *
                    ((int32_t)cal.dig_T3) >> 14;

    t_fine = var1 + var2;
    return (t_fine * 5 + 128) / 25600.0f;
}

static float compensate_P(uint32_t adc_P)
{
    int64_t var1 = ((int64_t)t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)cal.dig_P6;
    var2 = var2 + ((var1 * (int64_t)cal.dig_P5) << 17);
    var2 = var2 + (((int64_t)cal.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)cal.dig_P3) >> 8) + ((var1 * (int64_t)cal.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)cal.dig_P1) >> 33;

    if (var1 == 0) return 0.0f;

    int64_t p = 1048576 - (int64_t)adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)cal.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)cal.dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)cal.dig_P7) << 4);

    return (float)p / 256.0f; // Pa
}



static float compensate_H(uint16_t adc_H)
{
    int32_t v_x1 = t_fine - 76800;

    v_x1 = (((((adc_H << 14) - ((int32_t)cal.dig_H4 << 20) -
              ((int32_t)cal.dig_H5 * v_x1)) + 16384) >> 15) *
            (((((((v_x1 * (int32_t)cal.dig_H6) >> 10) *
               (((v_x1 * (int32_t)cal.dig_H3) >> 11) + 32768)) >> 10) +
               2097152) * (int32_t)cal.dig_H2 + 8192) >> 14));

    v_x1 = v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) *
            (int32_t)cal.dig_H1) >> 4);

    if (v_x1 < 0) v_x1 = 0;
    if (v_x1 > 419430400) v_x1 = 419430400;

    return (v_x1 >> 12) / 1024.0f;
}

//---------------------tsl 2561 driver-------------------------

//-------------TSL  WRITE HELPER FUNCTION------------
static esp_err_t tsl_write8(uint8_t reg,uint8_t val)
{
	uint8_t buff[2] = {(uint8_t)(0x80| reg), val};
	return i2c_master_transmit(TSL2561_handle,buff,2,pdMS_TO_TICKS(100));
}

//-------------TSL  READ HELPER FUCTION------
static esp_err_t tsl_read8(uint8_t reg, uint8_t *out)
{
    uint8_t cmd = 0x80 | reg;

    return i2c_master_transmit_receive(
        TSL2561_handle,
        &cmd, 1,          // write register
        out, 1,           // read 1 byte
        pdMS_TO_TICKS(100)
    );
}

			
//-----------tsl2561 config-------------
static esp_err_t tsl_configure(void){
	ESP_ERROR_CHECK(tsl_write8(0x00,0x03));//POWER ON
	ESP_ERROR_CHECK(tsl_write8(0x01,0x02)); // timing
	vTaskDelay(pdMS_TO_TICKS(500));
	return ESP_OK; 
}


//--------I2C MASTER BUS INIT--------
			
static esp_err_t i2c_test_init(void){
i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));


i2c_device_config_t tsl_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = TSL2561_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ_HZ,
};


i2c_device_config_t bme_cfg = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = BME280_ADDR,
	.scl_speed_hz = I2C_MASTER_FREQ_HZ,
};
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle,&bme_cfg,&BME280_handle));
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &tsl_cfg, &TSL2561_handle));
tsl_configure();
bme_configure();
cal_reg();
return ESP_OK;
}
//--- function to calculate visible light 
float calculate_lux(uint16_t ch0, uint16_t ch1)
{
    if (ch0 == 0) return 0.0f;

    float ratio = (float)ch1 / (float)ch0;
    float lux = 0.0f;

    if (ratio <= 0.5f) {
        lux = (0.0304f * ch0) - (0.062f * ch0 * powf(ratio, 1.4f));
    } else if (ratio <= 0.61f) {
        lux = (0.0224f * ch0) - (0.031f * ch1);
    } else if (ratio <= 0.80f) {
        lux = (0.0128f * ch0) - (0.0153f * ch1);
    } else if (ratio <= 1.30f) {
        lux = (0.00146f * ch0) - (0.00112f * ch1);
    } else {
        lux = 0.0f;
    }

    return lux;
}


//-----------------sensor task-----------

void sensor_task(void *arg){
	ESP_ERROR_CHECK(i2c_test_init());
    adc_oneshot_unit_handle_t adc_handle;

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };

    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
    //---------------------------- soil moisture-----------------------
        int sum = 0;
        sensor_data_t data;
        for (int i = 0; i < SAMPLES; i++) {
            int raw;
            ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw));
            sum += raw;
        }

        float averaged_raw = (float)sum / SAMPLES;
        float calculated_mv = (averaged_raw * 3300.0f) / 4095.0f;

	float moist_pct = (100.0/4095.0)* averaged_raw ;
    data.soil_moisture= moist_pct;

    //--------------------------------------light, humidity ,pressue,tempertaure --------------
    /* ============================= */
/*        TSL2561 Light          */
/* ============================= */

/* --- Channel 0: IR + Visible --- */
uint8_t ch0_low, ch0_high;

tsl_read8(0x0C, &ch0_low);
tsl_read8(0x0D, &ch0_high);

uint16_t CH0 = ((uint16_t)ch0_high << 8) | ch0_low;

/* --- Channel 1: IR only --- */
uint8_t ch1_low, ch1_high;

tsl_read8(0x0E, &ch1_low);
tsl_read8(0x0F, &ch1_high);   // fixed register

uint16_t CH1 = ((uint16_t)ch1_high << 8) | ch1_low;

/* --- Lux Calculation --- */
float lux = calculate_lux(CH0, CH1);

printf("Light lux = %.2f\n", lux);
data.lux = lux;


/* ============================= */
/*            BME280             */
/* ============================= */

uint8_t d[8];

bme_readN(0xF7, d, 8);

/* --- Raw ADC Extraction --- */
uint32_t adc_P = ((uint32_t)d[0] << 12) |
                 ((uint32_t)d[1] << 4)  |
                 ((uint32_t)d[2] >> 4);

uint32_t adc_T = ((uint32_t)d[3] << 12) |
                 ((uint32_t)d[4] << 4)  |
                 ((uint32_t)d[5] >> 4);

uint16_t adc_H = ((uint16_t)d[6] << 8) |
                  (uint16_t)d[7];

/* --- Compensation --- */
float temp_c   = compensate_T(adc_T);
float press_pa = compensate_P(adc_P);
float hum_pct  = compensate_H(adc_H);

float press_hpa = press_pa / 100.0f;



data.pressure = press_pa;
data.temperature =temp_c;
data.humidity = hum_pct;

printf("Temp: %.2f C, Hum: %.2f %%, Pressure: %.2f hPa\n",                                                              
        temp_c, hum_pct, press_hpa); 
        
        xQueueSend(s_out, &data, portMAX_DELAY);

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_MS));
    }
}





void sensor_task_start(QueueHandle_t out_queue)
{
    s_out = out_queue;
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}



