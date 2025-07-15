#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "lcd.h"
#include "freertos.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define IN3 6                           // Dirección 1 puente H
#define IN4 7                           // Dirección 2 puente H
#define ENB 8                           // Enable puente H (PWM)
#define SDA 26                          // Pin SDA display LCD
#define SCL 27                          // Pin SCL display LCD
#define MISO 4                          // Pin MISO del SD
#define CS 1                            // Pin CS del SD
#define SCK 2                           // Pin SCK del SD
#define MOSI 3                          // Pin MOSI del SD
#define SENSOR_IR 18                    // Salida del sensor IR
#define SENTIDO 15                      // Botón para cambio de sentido
#define CALIBRAR 14                     // Botón para ajustar PID
#define CLK 13                          // Pin CLK del encoder
#define DT 12                           // Pin DT del encoder
#define SW 11                           // Pin del botón del encoder

#define T_MUESTREO 120                  // Tiempo de muestreo en ms
#define RANURAS 50.f                     // Cantidad de ranuras en el disco
#define T_ANTIRREBOTE 10                // Tiempo de antirrebote
#define T_PRESIONADO 1000               // Tiempo para mantener presionado el boton del encoder
#define FRECUENCIA 1000                 // Frecuencia en Hz
#define DC 500                          // Cilco de actividad en % SOLO PRUEBA

typedef struct {                        // Estructura para la cola del encoder
    bool clk;                           // Guarda el estado de CLK
    bool dt;                            // Guarda el estado de DT
} encoder_t;                            

SemaphoreHandle_t semaforo_contador;    // Contador para velocidad 
SemaphoreHandle_t semaforo_sw;          // Semaforo para el switch del encoder
QueueHandle_t cola_encoder;             // Cola para estados del encoder
QueueHandle_t cola_dc;                  // Cola con el duty cycle SOLO PRUEBAS
QueueHandle_t cola_velocidad_medida;    // Cola con la velocidad medida

/**
 * @brief Interrupción del GPIO, se evaluan todas las interrupciones
 */
void gpio_irq_handler(uint gpio, uint32_t event_mask) {
    BaseType_t to_higher_priority_task;
    encoder_t encoder;

    switch(gpio) {
    case SENSOR_IR: // IRQ para la velocidad
        xSemaphoreGiveFromISR(semaforo_contador, &to_higher_priority_task);
        break;
    case SW: // IRQ para el boton del encoder
        gpio_set_irq_enabled(SW, event_mask, false);
        xSemaphoreGiveFromISR(semaforo_sw, &to_higher_priority_task);
        break;
    case CLK: // IRQ para el CLK del encoder
        encoder.clk = gpio_get(CLK);
        encoder.dt = gpio_get(DT);
        xQueueOverwriteFromISR(cola_encoder, &encoder, &to_higher_priority_task);
        break;
    }
    portYIELD_FROM_ISR(to_higher_priority_task);
}

/**
 * @brief Tarea que calcula la velocidad y la envía por una cola
 * @todo Tarea que lea esta velocidad y la procese 
 */
void task_velocidad(void *params) {
    uint16_t velocidad_rpm, pulsos;
    TickType_t ultimo_tick = xTaskGetTickCount();
    while(1) {
        vTaskDelayUntil(&ultimo_tick, pdMS_TO_TICKS(T_MUESTREO)); // Espera el tiempo de muestra
        //vTaskDelay(pdMS_TO_TICKS(T_MUESTREO)); // Espera el tiempo de muestre
        pulsos = uxSemaphoreGetCount(semaforo_contador); // Leemos los pulsos
        xQueueReset(semaforo_contador); // Vaciamos el contador
        velocidad_rpm = (uint16_t)(((float)pulsos / RANURAS) * (60000.0 / T_MUESTREO)); // Calculamos la velocidad
        printf("%d %d\n", pulsos, velocidad_rpm);
        //xQueueSendToBack(cola_velocidad_medida, &velocidad_rpm, portMAX_DELAY); // Mandamos por la cola
        xQueueOverwrite(cola_velocidad_medida, &velocidad_rpm); // SOLO PARA PRUEBA
    }
}

/**
 * 
 */
void task_mostrar(void *params) {
    uint16_t velocidad_rpm;
    uint16_t dc;
    char vel[4];
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(500));
        xQueuePeek(cola_velocidad_medida, &velocidad_rpm, portMAX_DELAY);
        xQueuePeek(cola_dc, &dc, portMAX_DELAY);
        lcd_set_cursor(0, 7);
        sprintf(vel, "%4d", velocidad_rpm);
        lcd_string(vel);
        lcd_set_cursor(1, 4);
        sprintf(vel, "%4d", dc);
        lcd_string(vel);
    }
}

/**
 * @brief Tarea para leer el boton del encoder, tanto presionado como mantenido
 * @todo Menus/Tareas que procesen el pulsador
 */
void task_switch(void *params) {
    while(1) {
        xSemaphoreTake(semaforo_sw, portMAX_DELAY); // Esperamos el semaforo
        vTaskDelay(pdMS_TO_TICKS(T_ANTIRREBOTE)); // Esperamos el tiempo de rebote
        gpio_set_irq_enabled(SW, GPIO_IRQ_EDGE_RISE, true); // Activamos la interrupcion por flanco ascendende para soltar el boton
        if(xSemaphoreTake(semaforo_sw, pdMS_TO_TICKS(T_PRESIONADO))== pdTRUE) { 
            printf("Boton presionado\n"); // Si se recibio el semaforo antes de que paso el tiempo de presionado se lee como pulsacion
        }
        else {
            printf("Boton mantenido\n"); // Si se paso del tiempo se lee como mantenido
            xSemaphoreTake(semaforo_sw, portMAX_DELAY); // Se espera a que suelte el boton 
        }
        vTaskDelay(pdMS_TO_TICKS(T_ANTIRREBOTE)); // Espera el tiempo de rebote
        gpio_set_irq_enabled(SW, GPIO_IRQ_EDGE_FALL, true); // Reactiva el ciclo de interrupcion
        printf("Boton soltado\n");
    }
}

/**
 * @brief Tarea que lee la rotacion del encoder
 * @todo Tarea que actualice datos con estos valores
 */
void task_encoder(void *params) {
    encoder_t actual, anterior = {0, 0};
    int32_t cuenta = 0;
    uint16_t dc = DC;
    TickType_t ultimo_tick = xTaskGetTickCount(), tick_actual;
    while (1) {
        xQueueReceive(cola_encoder, &actual, portMAX_DELAY); // Espera la cola del encoder
        tick_actual = xTaskGetTickCount(); // Se verifican los ticks que pasaron
        // Se verifica el rebote comparando el tick anterior con el actual
        if ((tick_actual - ultimo_tick) >= pdMS_TO_TICKS(T_ANTIRREBOTE)) {
            if (actual.clk != anterior.clk) { // Verificamos si cambio del estado del CLK
                if (actual.clk != actual.dt) {
                    //cuenta++;  // Giro horario
                    //printf("Horario     %d\n", cuenta);
                    printf("Horario     %d\n", dc);
                    if(dc<1000) {
                        dc+=10;
                    }
                } 
                else {
                    //cuenta--;  // Giro antihorario
                    //printf("Antihorario %d\n", cuenta);
                    printf("Antihorario     %d\n", dc);
                    if(dc>0) {
                        dc-=10;
                    }
                }
                xQueueOverwrite(cola_dc, &dc);
                pwm_set_gpio_level(ENB, dc); // SOLO PRUEBA
                ultimo_tick = tick_actual; // Actualizamos el tick
            }
            anterior = actual; // Actualizamos el estado de CLK
        }
    }
}

int main() {
    stdio_init_all();

    // Inicializacion pin sensor infrarrojo
    gpio_init(SENSOR_IR);
    gpio_set_dir(SENSOR_IR, GPIO_IN);

    // Inicializacion pin CLK del encoder
    gpio_init(CLK);
    gpio_set_dir(CLK, GPIO_IN);

    // Inicializacion pin DT del encoder
    gpio_init(DT);
    gpio_set_dir(DT, GPIO_IN);

    // Inicializacion pin SW del encoder
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);

    // Inicializacion pin CLK del encoder
    gpio_init(IN3);
    gpio_set_dir(IN3, GPIO_OUT);
    // Ponemos en 1 ES SOLO PRUEBA
    // SOLO DE PRUEBA
    // SOLO DE PRUEBA
    gpio_put(IN3, 0);
    
    // Inicializacion pin CLK del encoder
    gpio_init(IN4);
    gpio_set_dir(IN4, GPIO_OUT);
    // Ponemos en 0 ES SOLO PRUEBA 
    // SOLO DE PRUEBA
    // SOLO DE PRUEBA
    gpio_put(IN4, 1);
    
    // Inicio PWM
    gpio_set_function(ENB, GPIO_FUNC_PWM);
    uint32_t slice = pwm_gpio_to_slice_num(ENB);
    pwm_set_clkdiv(slice, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS) / 1000.0);
    pwm_set_wrap(slice, 1000000 / FRECUENCIA);
    pwm_set_gpio_level(ENB, DC); // SOLO PRUEBA
    pwm_set_enabled(slice, true);

    // Inicio LCD
    // Inicializo el I2C con un clock de 100 KHz
    i2c_init(i2c1, 100000);
    // Habilito la funcion de I2C en los GPIOs
    gpio_set_function(SDA, GPIO_FUNC_I2C);
    gpio_set_function(SCL, GPIO_FUNC_I2C);
    // Habilito pull-ups
    gpio_pull_up(SDA);
    gpio_pull_up(SCL);
    // Inicializo LCD
    lcd_init(i2c1, 0x27);
    // Limpio pantalla
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_string("Vel:         RPM");
    lcd_set_cursor(1, 0);
    lcd_string("DC:   ");

    // Habilitacion de interrupciones GPIO
    gpio_set_irq_enabled_with_callback(SENSOR_IR, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
    gpio_set_irq_enabled(CLK, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(SW, GPIO_IRQ_EDGE_FALL, true);

    // Creacion de semaforos y colas
    semaforo_sw = xSemaphoreCreateBinary();
    semaforo_contador = xSemaphoreCreateCounting(400, 0);
    cola_encoder = xQueueCreate(1, sizeof(encoder_t));
    cola_velocidad_medida = xQueueCreate(1, sizeof(uint16_t));
    cola_dc = xQueueCreate(1, sizeof(uint16_t));
    uint16_t muestra = DC;
    xQueueOverwrite(cola_dc, &muestra);

    // Creacion de tareas
    xTaskCreate(
        task_velocidad, 
        "task_velocidad", 
        configMINIMAL_STACK_SIZE * 2, 
        NULL, 
        tskIDLE_PRIORITY + 2, 
        NULL
    );

    xTaskCreate(
        task_switch,
        "task_switch",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    xTaskCreate(
        task_encoder,
        "task_encoder",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    xTaskCreate(
        task_mostrar,
        "task_mostrar",
        configMINIMAL_STACK_SIZE*2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );


    // Inicio del Scheduler
    vTaskStartScheduler();

    while(1);
}
