#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/irq.h"

#include "freertos.h"
#include "task.h"
#include "queue.h"

// Handler de la cola
QueueHandle_t queue_sensor;

/**
 * @brief Interrupción del ADC
 */
void adc_irq_handler(void) {
    // Variable para cambio de contexto 
    BaseType_t to_higher_priority_task;
    // Leo el registro FIFO
    uint16_t sensor = adc_fifo_get();
    // Detengo el ADC
    adc_run(0);
    // Limpio el buffer 
    adc_fifo_drain();
    // Envio el dato a la cola
    xQueueSendToBackFromISR(queue_sensor, &sensor, &to_higher_priority_task);
    // Cambia el contexto si es necesario
    portYIELD_FROM_ISR(to_higher_priority_task);
}

/**
 * @brief Tarea que dispara la lectura del ADC
 */
void task_disparo_adc(void *params) {
    while(1) {
        // Disparo el ADC
        adc_run(1);
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
    // Configuro la FIFO para la interrupción con 1 dato
    adc_fifo_setup(1, 0, 1, 0, 0);
    // Habilito la interrupción del ADC
    adc_irq_set_enabled(1);
    // Especifico el handler de la interrupción
    irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_irq_handler);
    // Habilito la interrupción del ADC
    irq_set_enabled(ADC_IRQ_FIFO, 1);
    // Creo la cola con longitud 1
    queue_sensor = xQueueCreate(1, sizeof(uint16_t));
    // Creo la tarea de lectura del ADC
    xTaskCreate(
        task_disparo_adc, 
        "task_disparo_adc", 
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
        configMINIMAL_STACK_SIZE * 2, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL
    );
    // Inicio el scheduler
    vTaskStartScheduler();
    while(1) {
    }
}
