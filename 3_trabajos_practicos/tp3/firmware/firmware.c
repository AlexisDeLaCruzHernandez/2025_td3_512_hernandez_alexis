#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "helper.h"
// #include "lcd.h"
#include "freertos.h"
#include "task.h"
#include "semphr.h"

// Frecuencia del PWM en Hz
#define FRECUENCIA 10000
// Pin del PWM
#define PWM_PIN 14
// Pin de la entrada de señal
#define IN_PIN 15
// Cantidad de elementos del semaforo
#define ELEMENTOS 3000
// Tiempo de lectura en ms
#define TIEMPO 25

SemaphoreHandle_t contador;

/**
 * @brief Tarea que lee el pin de entrada
 */
void task_lectura(void *params) {
    while(1) {
        if(gpio_get(IN_PIN) == 1) {
            xSemaphoreGive(contador);
            while (gpio_get(IN_PIN) == 1);
        }
    }
}

/**
 * @brief Tarea que calcula y muestra la frecuencia
 */
void task_calculo_muestra(void *params) {
    uint8_t periodos;
    float frecuencia;
    while(1) {
        // Espero lecturas de la señal
        vTaskDelay(pdMS_TO_TICKS(TIEMPO));
        // Leo la cantidad de pulsos de la señal
        periodos = uxSemaphoreGetCount(contador);
        // Vacio el contador para una nueva lectura
        xQueueReset(contador);
        // Calculo la frecuencia en kHz
        frecuencia = (float) periodos / TIEMPO;
        // Muestro la frecuencia por consola
        printf("Frecuencia = %3.2fkHz\n", frecuencia);
    }
}


int main()
{
    stdio_init_all();
    // Inicializo el pin de entrada
    gpio_init(IN_PIN);
    // Configuro el pin como entrada
    gpio_set_dir(IN_PIN, GPIO_IN);
    // Inicializo el PWM
    pwm_user_init(PWM_PIN, FRECUENCIA);
    // Creo un semaforo contador
    contador = xSemaphoreCreateCounting(ELEMENTOS, 0);
    // Creo la tarea de lectura
    xTaskCreate(
        task_lectura, 
        "task_lectura", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL
    );
    // Creo la tarea de calculo y muestra
    xTaskCreate(
        task_calculo_muestra, 
        "task_calculo_muestra", 
        configMINIMAL_STACK_SIZE * 2, 
        NULL, 
        tskIDLE_PRIORITY + 2, 
        NULL
    );
    // Inicio el scheduler
    vTaskStartScheduler();

    while(1) {
    }
}
