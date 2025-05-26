#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "freertos.h"
#include "task.h"
#include "queue.h"

// Handler de la cola
QueueHandle_t queue_sensor;

/**
 * @brief Tarea de lectura del sensor de temperatura
 */
void task_lectura(void *params) {
    uint16_t sensor;
    while(1) {
        // Leo el sensor
        sensor = adc_read();
        // Envio el dato a la cola
        xQueueSendToBack(queue_sensor, &sensor, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(500));  
    }
}

/**
 * @brief Tarea de escritura de la temperatura por consola
 */
void task_escritura(void *params) {
    uint16_t sensor;
    float temperatura;
    while(1) {
        // Leo el dato de la cola
        xQueueReceive(queue_sensor, &sensor, portMAX_DELAY);
        // Calculo la temperatura en grados celcius
        temperatura = 27 - (sensor * 3.3 / 4095 - 0.706) / (0.001721);
        printf("Temperatura: %f°C\n", temperatura);
    }
}

int main() {
    stdio_init_all();
    // Inicializa el ADC
    adc_init();
    // Habilito el sensor de temperatura
    adc_set_temp_sensor_enabled(1);
    // Selecciono el sensor de temperatura
    adc_select_input(4);
    // Creo la cola con longitud 1
    queue_sensor = xQueueCreate(1, sizeof(uint16_t));
    // Creo la tarea de lectura del ADC
    xTaskCreate(
        task_lectura, 
        "task_lectura", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL
    );
    // Creo la tarea de escritura de la temperatura por consola
    // No alcanzaba la memoria con el minimo stack
    xTaskCreate(
        task_escritura, 
        "task_escritura", 
        configMINIMAL_STACK_SIZE*2, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL
    );
    // Inicio el scheduler
    vTaskStartScheduler();
    while(1) {
    }
}
