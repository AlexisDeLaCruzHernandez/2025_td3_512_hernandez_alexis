#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"

#include "lcd.h"
#include "bmp280.h"
#include "freertos.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define SDA_PIN 16 // Pin del SDA
#define SCL_PIN 17 // Pin del SCL
#define IN_PIN 14 // Pin del botón
#define PWM_PIN 15 // Pin del PWM
#define ADDRESS 0x27 // Address del adaptador del LCD
#define TIEMPO_PANTALLA 500 //ms entre actualizacion de pantalla
#define TIEMPO_MEDICION 200 //ms entre mediciones del sensor
#define REBOTE 50 //ms de rebote
#define T_SET 25.0 //set point de temperatura

typedef struct {
    int32_t temperatura;
    int32_t presion;
} sensor_raw_t; //estructura para la cola de datos crudos

typedef struct {
    float temperatura;
    float presion;
} sensor_t; //estructura para la cola de datos procesados

QueueHandle_t queue_sensor_raw; //cola para datos crudos
QueueHandle_t queue_sensor; //cola para datos procesados
QueueHandle_t queue_mensaje; //cola que informa a la tarea guardiana
SemaphoreHandle_t cambio_pantalla; //semaforo para el cambio de pantalla

/**
 * @brief Interrupción del botón para cambiar la pantalla
 */
void gpio_irq_handler(uint gpio, uint32_t event_mask) {
    // Verifico la interrupcion
    if (event_mask == GPIO_IRQ_EDGE_RISE) {
        BaseType_t to_higher_priority_task;
        // Doy el semaforo para cambiar de pantalla
        xSemaphoreGiveFromISR(cambio_pantalla, &to_higher_priority_task);
        portYIELD_FROM_ISR(to_higher_priority_task);
    }
}

/**
 * @brief Tarea guardiana que escribe el I2C
 */
void task_i2c(void *params) {
    char linea_1[16], linea_2[16];
    sensor_raw_t sensor_raw;
    sensor_t sensor;
    uint8_t msg;

    while(1) {
        xQueueReceive(queue_mensaje, &msg, portMAX_DELAY);

        if(msg == 0) {
            bmp280_read_raw(&sensor_raw.temperatura, &sensor_raw.presion);
            xQueueSendToBack(queue_sensor_raw, &sensor_raw, pdMS_TO_TICKS(1));
        }

        if(msg == 1 || msg == 2) {
            xQueueReceive(queue_sensor, &sensor, portMAX_DELAY);
            if (msg == 1) {
                sprintf(linea_1, "T: %5.2f C      ", sensor.temperatura);
                sprintf(linea_2, "P: %7.3fkPa   ", sensor.presion);
            }
            else {
                sprintf(linea_1, "T_SET: %5.2f C  ", T_SET);
                sprintf(linea_2, "Error: %5.2f C  ", fabs(T_SET - sensor.temperatura));
            }
            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_string(linea_1);
            lcd_set_cursor(1, 0);
            lcd_string(linea_2);
        }
    }
}

/**
 * @brief Tarea que pide la lectura del sensor a la tarea guardiana
 */
void task_bmp280(void *params) {
    uint8_t msg = 0;

    while(1) {
        xQueueSendToBack(queue_mensaje, &msg, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(TIEMPO_MEDICION));
    }
}

/**
 * @brief Tarea que pide actualizar el lcd a la tarea guardiana
 */
void task_lcd(void *params) {
    uint8_t msg = 1;
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

    while(1) {
        if(xSemaphoreTake(cambio_pantalla, pdMS_TO_TICKS(TIEMPO_PANTALLA)) == pdTRUE) {
            gpio_set_irq_enabled(IN_PIN, GPIO_IRQ_EDGE_RISE, false);
            vTaskDelay(pdMS_TO_TICKS(REBOTE));
            gpio_set_irq_enabled(IN_PIN, GPIO_IRQ_EDGE_RISE, true);
            if(msg == 1) {
                msg = 2;
                pwm_set_enabled(slice_num, true);
            }
            else {
                msg = 1;
                pwm_set_enabled(slice_num, false);
            }
        }
        xQueueSendToBack(queue_mensaje, &msg, portMAX_DELAY);
    }
}

void task_procesado(void *params) {
    struct bmp280_calib_param parametros;
    sensor_raw_t sensor_raw;
    sensor_t sensor;
    uint8_t duty;

    //inicializo el PWM
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    uint channel = pwm_gpio_to_channel (PWM_PIN);
    //125 niveles de resolución para la intensidad del LED
    //el rango de temperatura es de -40 a 85 grados
    pwm_set_wrap(slice_num, 125);
    pwm_set_chan_level(slice_num, channel, 0);
    //desactivo el pwm
    pwm_set_enabled(slice_num, false);

    //obtengo los parametros de calibracion
    bmp280_get_calib_params(&parametros);

    while(1) {
        xQueueReceive(queue_sensor_raw, &sensor_raw, portMAX_DELAY);
        
        //conviero la temperatura y presion
        sensor.temperatura = bmp280_convert_temp(sensor_raw.temperatura, &parametros);
        sensor.presion = bmp280_convert_pressure(sensor_raw.presion, sensor_raw.temperatura, &parametros)/1000.0;

        xQueueSendToBack(queue_sensor, &sensor, portMAX_DELAY);

        duty = (uint8_t)fabs(T_SET - sensor.temperatura);

        pwm_set_chan_level(slice_num, channel, duty);
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

    // Inicializo el botón
    gpio_init(IN_PIN);
    gpio_set_dir(IN_PIN, GPIO_IN); //configuro como entrada
    gpio_set_irq_enabled_with_callback(IN_PIN, GPIO_IRQ_EDGE_RISE, true, gpio_irq_handler); //interrupcion

    //inicialización del LCD
    lcd_init(i2c0, ADDRESS); 

    //inicializacion del BMP280
    bmp280_init(i2c0);

    //creo la cola para los datos de temperatura y presion crudos
    queue_sensor_raw = xQueueCreate(1, sizeof(sensor_raw_t));

    //creo la cola para los datos de temperatura y presion procesados
    queue_sensor = xQueueCreate(1, sizeof(sensor_t));

    //creo la cola para los mensajes a la tarea guardiana
    queue_mensaje = xQueueCreate(3, sizeof(uint8_t));

    //creo un semaforo binario para cambiar de pantalla
    cambio_pantalla = xSemaphoreCreateBinary();

    //tarea guardiana
    xTaskCreate(task_i2c, "task_i2c", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);

    //tarea del bmp280
    xTaskCreate(task_bmp280, "task_bmp280", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

    //tarea de la pantalla 1
    xTaskCreate(task_lcd, "task_lcd", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

    //tarea de procesamiento del sensor
    xTaskCreate(task_procesado, "task_procesado", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);

    // Inicio el scheduler
    vTaskStartScheduler();

    while(1);
}
