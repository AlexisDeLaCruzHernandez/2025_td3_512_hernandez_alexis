#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "gpio_driver.h"

// Etiqueta para el autor del modulo
#define AUTHOR	"Alexis Hernandez"

#define LED_PIN 21

// Puntero para primer hilo
static struct task_struct *thread1_on;
// Puntero para segundo hilo
static struct task_struct *thread2_off;
static uint8_t led_pin = LED_PIN;



/**
 * @brief Envia el mensaje "Hola desde el kernel!" cada 500ms
 */
static int thread1_on_f(void *params) {
	uint8_t led = *(uint8_t*) params;
	msleep(500);
	while(!kthread_should_stop()) {
		gpio_set(led);
		// Mensaje cuando corre la tarea
		printk(KERN_INFO "%s: ON\n", AUTHOR);
		// Demora de medio segundo
		msleep(1000);
	}
	return 0;
}

/**
 * @brief Envia el mensaje "Chau desde el kernel!" cada 500ms
 */
static int thread2_off_f(void *params) {
	uint8_t led = *(uint8_t*) params;
	while(!kthread_should_stop()) {
		gpio_clr(led);
		// Mensaje cuando corre la tarea
		printk(KERN_INFO "%s: OFF\n", AUTHOR);
		// Demora de medio segundo
		msleep(1000);
	}
	return 0;
}

/**
 * @brief Se llama cuando el modulo se carga en el kernel
*/
static int __init kernel_module_init(void) {

	if(gpio_map() == NULL) {
		printk(KERN_ERR "%s: Error al solicitar memoria virtual\n", AUTHOR);
		return -1;
	}
	gpio_set_dir_output(led_pin);
	// Mensaje para el kernel
	printk(KERN_INFO "%s: Insertando el modulo de kernel\n", AUTHOR);
	// Creacion de tarea 1
	thread1_on = kthread_run(
		thread1_on_f,
		(void*) &led_pin,
		"thread1_on"
	);
	// Verificacion de error
	if(IS_ERR(thread1_on)) {
		printk(KERN_ERR "%s: Error al crear el hilo thread1_on\n", AUTHOR);
		return -1;
	}
	// Creacion de tarea 2
	thread2_off = kthread_run(
		thread2_off_f,
		(void*) &led_pin,
		"thread2_off"
	);
	// Verificacion de error
	if(IS_ERR(thread2_off)) {
		printk(KERN_ERR "%s: Error al crear el hilo thread2_off\n", AUTHOR);
		// Eliminamos la tarea anterior
		kthread_stop(thread1_on);
		return -1;
	}
	return 0;
}

/**
 * @brief Se llama cuando el modulo se quita del kernel
 */
static void __exit kernel_module_exit(void) {
	// Mensaje
	pr_info("%s: Removiendo el modulo del kernel\n", AUTHOR);
	// Eliminamos los hilos creados
	if(thread1_on) {
		kthread_stop(thread1_on);
	}
	if(thread2_off) {
		kthread_stop(thread2_off);
	}
}

// Registro la funcion de inicializacion y salida
module_init(kernel_module_init);
module_exit(kernel_module_exit);

// Informacion del modulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION("Trabajo practico 5 v2");
