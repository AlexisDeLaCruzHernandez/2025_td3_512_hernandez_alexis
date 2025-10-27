#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>

// Autor del modulo
#define AUTHOR "Hernandez-Jorja"

// Minor number del char device
#define CHRDEV_MINOR 1
// Cantidad de char devices
#define CHRDEV_COUNT 1

// Variable que guarda los major y minor numbers del char device
static dev_t chrdev_number;
// Variable que representa el char device
static struct cdev chrdev;
// Clase del char device
static struct class *chrdev_class;

// Buffer de datos para compartir entre user y kernel
static char shared_buffer[128];

// Prototipos de los callbacks
static ssize_t chr_dev_read(struct file *f, char __user *buff, size_t size, loff_t *off);
static ssize_t chr_dev_write(struct file *f, const char __user *buff, size_t size, loff_t *off);

// Operaciones de archivos del char device
static struct file_operations chrdev_ops = {
    .owner = THIS_MODULE,
    .read = chr_dev_read,
    .write = chr_dev_write
};

/**
 * @brief Operacion si se lee el char device
 */
static ssize_t chr_dev_read(struct file *f, char __user *buff, size_t size, loff_t *off) {
    // Variables auxiliares
    int to_copy, not_copied, copied;
    // Si el offset es mas grande al buffer compartido no hay
    // mas para leer -> return 0 para indicar EOF
    if(*off >= sizeof(shared_buffer)) return 0;
    // Se fija cuanto hay que copiar, fijandose si la cantidad
    // a leer o lo que queda del buffer es menor. Evita leer 
    // fuera de los limites del buffer
    to_copy = min(size, sizeof(shared_buffer) - *off);
    // Copia del kernel space al user space, devuelve cuanto no se copio
    not_copied = copy_to_user(buff, shared_buffer + *off, to_copy);
    // actualiza el offset
    *off += to_copy;
    // Calcula cuanto se copio
    copied = to_copy - not_copied;
    // Retorna la cantidad copiada
    return copied;
}

/**
 * @brief Operacion si se escribe el char device
 */
static ssize_t chr_dev_write(struct file *f, const char __user *buff, size_t size, loff_t *off) {
    // Variables auxiliares
    int to_copy, not_copied, copied;
    // Se fija cuanto puede copiar sin exceder el shared buffer
    to_copy = min(size, sizeof(shared_buffer) - 1);
    // Copia del user space al kernel space, devuelve cuanto no se copio
    not_copied = copy_from_user(shared_buffer, buff, to_copy);
    // Evalua cuanto se copio efectivamente
    copied = to_copy - not_copied;
    // Pone fin de palabra si hay un \n
    for(int i = 0; i < copied; i++) {
        if(shared_buffer[i] == '\n') {
            shared_buffer[i] = '\0';
            break;
        }
    }
    // Muestra que se escribio en el kernel
    printk(KERN_INFO "%s: Se escribio '%s' en el char device\n", AUTHOR, shared_buffer);
    // Retorna la cantidad copiada
    return copied;
}

/**
 * @brief Crea el char device
 * @return Devuelve cero si la inicializacion fue correcta
 */
static int __init module_kernel_init(void) {
    // Reservar char device
    if(alloc_chrdev_region(&chrdev_number, CHRDEV_MINOR, CHRDEV_COUNT, AUTHOR) < 0) {
        printk(KERN_ERR "%s: No se pudo crear el char device\n", AUTHOR);
        return -1;
    }
    // Mensaje para buscar el char device
    printk(KERN_INFO "%s: Se reservo char device con major %d y minor %d\n", AUTHOR, MAJOR(chrdev_number), MINOR(chrdev_number));
    // Inicializa el char device y sus operaciones de archivos
    cdev_init(&chrdev, &chrdev_ops);
    // Asocia el char device a la zona reservada
    if(cdev_add(&chrdev, chrdev_number, CHRDEV_COUNT) < 0) {
        unregister_chrdev_region(chrdev_number, CHRDEV_COUNT);
        printk(KERN_ERR "%s: No se pudo crear el char device\n", AUTHOR);
        return -1;
    }
    // Crea la estructura de clase
    chrdev_class = class_create(AUTHOR);
    // Verifica error
    if(IS_ERR(chrdev_class)) {
        unregister_chrdev_region(chrdev_number, CHRDEV_COUNT);
        printk(KERN_ERR "%s: No se pudo crear el char device\n", AUTHOR);
        return -1;
    }
    // Se crea el archivo del char device
    if(IS_ERR(device_create(chrdev_class, NULL, chrdev_number, NULL, AUTHOR))) {
        class_destroy(chrdev_class);
        unregister_chrdev_region(chrdev_number, CHRDEV_COUNT);
        printk(KERN_ERR "%s: No se pudo crear el char device\n", AUTHOR);
        return -1;
    }
    // Mensaje de correcta finalizacion
    printk(KERN_INFO "%s: Fue creado el char device\n", AUTHOR);
    return 0;
}

/**
 * @brief Libera el espacio reservado del char device
 */
static void __exit module_kernel_exit(void) {
    device_destroy(chrdev_class, chrdev_number);
    class_destroy(chrdev_class);
    unregister_chrdev_region(chrdev_number, CHRDEV_COUNT);
    cdev_del(&chrdev);
    printk(KERN_INFO "%s: Modulo removido\n", AUTHOR);
}

// Funciones de inicializacion y salida
module_init(module_kernel_init);
module_exit(module_kernel_exit);

// Informacion del modulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION("Modulo de kernel EGB");
