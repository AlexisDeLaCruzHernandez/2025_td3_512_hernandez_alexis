#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "lcd.h"
#include "bmp280.h"
#include "freertos.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define SDA_PIN 16 // Pin del SDA
#define SCL_PIN 17 // Pin del SCL
#define ADDRESS 0x27 // Address del adaptador del LCD
#define TIEMPO 500 //ms entre mediciones

typedef struct {
    float temperatura;
    float presion;
} sensor_t;

QueueHandle_t queue_sensor;
SemaphoreHandle_t mutex_i2c;

void task_bmp280(void *params) {
    int32_t raw_temperatura, raw_presion;
    sensor_t datos;
    struct bmp280_calib_param parametros;

    //toma el mutex para usar el i2c
    xSemaphoreTake(mutex_i2c, portMAX_DELAY);
    bmp280_get_calib_params(&parametros);
    //deja el mutex
    xSemaphoreGive(mutex_i2c);

    while(1) {
        //toma el mutex para usar el i2c
        xSemaphoreTake(mutex_i2c, portMAX_DELAY);
        //leo la temperatura y presion
        bmp280_read_raw(&raw_temperatura, &raw_presion);
        //deja el mutex
        xSemaphoreGive(mutex_i2c);

        //conviero la temperatura y presion
        datos.temperatura = bmp280_convert_temp(raw_temperatura, &parametros);
        datos.presion = bmp280_convert_pressure(raw_presion, raw_temperatura, &parametros)/1000.0;

        //envio los datos por la cola
        xQueueSendToBack(queue_sensor, &datos, portMAX_DELAY);
    }
}

void task_lcd(void *params) {
    sensor_t datos;
    char temperatura[10];
    char presion[13];

    while(1) {
        //leo los datos del sensor
        xQueueReceive(queue_sensor, &datos, portMAX_DELAY);

        //defino el mensaje de la pantalla lcd
        sprintf(temperatura, "T: %5.2f C", datos.temperatura);
        sprintf(presion, "P: %7.3fkPa", datos.presion);

        //tomo el mutex para usar el i2c
        xSemaphoreTake(mutex_i2c, portMAX_DELAY);
        //Escribo en el LCD
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_string(temperatura);
        lcd_set_cursor(1, 0);
        lcd_string(presion);
        //espero TIEMPO para la siguiente medicion
        vTaskDelay(pdMS_TO_TICKS(TIEMPO));
        //dejo el mutex
        xSemaphoreGive(mutex_i2c);
    }
}


int main() {
    stdio_init_all();

    // Inicializo el I2C
    i2c_init(i2c0, 100000); //100kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C); //pin SDA
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C); //pin SCL
    gpio_pull_up(SDA_PIN); //pull up del pin SDA
    gpio_pull_up(SCL_PIN); //pull up del pin SCL

    //inicialización del LCD
    lcd_init(i2c0, ADDRESS); 

    //inicializacion del BMP280
    bmp280_init(i2c0);

    //creo la cola para los datos de temperatura y presion
    queue_sensor = xQueueCreate(1, sizeof(sensor_t));

    //creo un semaforo mutex para el recurso de i2c
    mutex_i2c = xSemaphoreCreateMutex();

    //tarea del bmp280
    xTaskCreate(
        task_bmp280, 
        "task_bmp280", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL
    );

    //tarea del lcd
    xTaskCreate(
        task_lcd, 
        "task_lcd", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL
    );

    // Inicio el scheduler
    vTaskStartScheduler();

    while(1);
}
