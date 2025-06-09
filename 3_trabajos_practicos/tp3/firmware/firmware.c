#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "helper.h"
#include "lcd.h"
#include "freertos.h"
#include "task.h"
#include "semphr.h"

// Frecuencia del PWM en Hz
#define FRECUENCIA 10000
// Pin del PWM
#define PWM_PIN 14
// Pin de la entrada de señal
#define IN_PIN 15
// I2C que se usa
#define I2C i2c0
// Pin del SDA
#define SDA_PIN 16
// Pin del SCL
#define SCL_PIN 17
// Address del adaptador del LCD
#define ADDRESS 0x27
// Cantidad de elementos del semaforo
#define ELEMENTOS 5000
// Tiempo de lectura en ms
#define TIEMPO 250

SemaphoreHandle_t contador;

/**
 * @brief Interrupcion de la entrada
 */
void gpio_irq_handler(uint gpio, uint32_t event_mask) {
    // Verifico la interrupcion
    if (event_mask == GPIO_IRQ_EDGE_RISE) {
        BaseType_t to_higher_priority_task;
        // Cuento la interrupcion
        xSemaphoreGiveFromISR(contador, &to_higher_priority_task);
        portYIELD_FROM_ISR(to_higher_priority_task);
    }
}

/**
 * @brief Tarea que calcula y muestra la frecuencia
 */
void task_calculo_muestra(void *params) {
    uint16_t periodos;
    float frecuencia;
    char frec[5];
    while(1) { 
        // Habilito la interrupcion para la lectura
        gpio_set_irq_enabled(IN_PIN, GPIO_IRQ_EDGE_RISE, true);
        // Espero lecturas de la señal
        vTaskDelay(pdMS_TO_TICKS(TIEMPO));
        // Desactivo la interrupcion para la lectura
        gpio_set_irq_enabled(IN_PIN, GPIO_IRQ_EDGE_RISE, false);
        // Leo la cantidad de pulsos de la señal
        periodos = uxSemaphoreGetCount(contador);
        // Vacio el contador para una nueva lectura
        xQueueReset(contador);
        // Calculo la frecuencia en Hz
        frecuencia = ((float) periodos / TIEMPO) * 1000;
        sprintf(frec, "%5.0f", frecuencia);
        // Muestro la frecuencia por LCD
        lcd_set_cursor(1, 0);
        lcd_string(frec);
    }
}


int main()
{
    stdio_init_all();
    // Inicializo el pin de entrada
    gpio_init(IN_PIN);
    // Configuro el pin como entrada
    gpio_set_dir(IN_PIN, GPIO_IN);
    // Habilito y le doy callback a la interrupcion
    gpio_set_irq_enabled_with_callback(IN_PIN, GPIO_IRQ_EDGE_RISE, true, gpio_irq_handler);
    gpio_set_irq_enabled(IN_PIN, GPIO_IRQ_EDGE_RISE, false);
    // Inicializo el PWM
    pwm_user_init(PWM_PIN, FRECUENCIA);
    // Creo un semaforo contador
    contador = xSemaphoreCreateCounting(ELEMENTOS, 0);
    // Inicializo el I2C
    i2c_init(I2C, 100000);
    // Habilito la funcion de I2C en los GPIOs
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    // Habilito pull-ups
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    // Inicializo LCD
    lcd_init(I2C, ADDRESS);
    // Escribo "Frecuencia:" en el LCD
    lcd_set_cursor(0, 0);
    lcd_string("Frecuencia:");
    // Escribo "Hz"
    lcd_set_cursor(1, 5);
    lcd_string("Hz");
    // Creo la tarea de calculo y muestra
    xTaskCreate(
        task_calculo_muestra, 
        "task_calculo_muestra", 
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
