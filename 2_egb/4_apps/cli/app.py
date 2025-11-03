import os, re, sys, termios

DEV_PATH = "/dev/Hernandez-Jorja" # Definimos ubicacion del archivo

def main():
    menu = 0
    msg = ""
    while True:
        match menu:
            # MENU PRINCIPAL
            case 0:
                match menu_principal():
                    case "1":
                        msg = "set"
                        menu = 1
                    case "2":
                        msg = "get"
                        menu = 2
                    case "3":
                        print("Está seguro:")
                        print("1> Si")
                        print("2> No")
                        match input("Opción [1-2]: ").strip():
                            case "1":
                                os.system("clear")
                                break
                    case _:
                        print("\nOpción invalida")
                        flush_stdin()
                        input("Presione tecla para continuar...")
            # MENU PARA SET
            case 1:
                opc = menu_set()
                match opc:
                    case "1": msg += " pkp"
                    case "2": msg += " pkd"
                    case "3": msg += " pki"
                    case "4": msg += " vob"
                    case "5": msg += " tac"
                    case "6": msg += " tde"
                    case "7": msg += " tim "
                    case "8": msg += " son"
                    case "9":  menu = 0
                    case _:
                        print("\nOpción invalida")
                        flush_stdin()
                        input("Presione tecla para continuar...")
                
                if opc in ["1", "2", "3", "4", "5", "6", "7", "8"]:
                    ok = 0
                    match opc:
                        case "1": 
                            while ok == 0:
                                print("-------------------------------------------")
                                print("Ingrese kp:")
                                flush_stdin()
                                val = float(input("[0.40 a 0.60]: "))
                                if 0.40 <= val <= 0.60:
                                    ok = 1
                                else:
                                    print("Formato inválido. Ej: 0.50")
                            val = round(val, 2)
                            msg = f"{msg} {val}" 
                        case "2": 
                            while ok == 0:
                                print("-------------------------------------------")
                                print("Ingrese kd:")
                                flush_stdin()
                                val = float(input("[0.0040 a 0.0060]: "))
                                if 0.0040 <= val <= 0.0060:
                                    ok = 1
                                else:
                                    print("Formato inválido. Ej: 0.0050")
                            val = round(val, 4)
                            msg = f"{msg} {val}" 
                        case "3": 
                            while ok == 0:
                                print("-------------------------------------------")
                                print("Ingrese ki:")
                                flush_stdin()
                                val = float(input("[0.60 a 0.90]: "))
                                if 0.6 <= val <= 0.9:
                                    ok = 1
                                else:
                                    print("Formato inválido. Ej: 0.8")
                            val = round(val, 2)
                            msg = f"{msg} {val}" 
                        case "4": 
                            while ok == 0:
                                print("-------------------------------------------")
                                print("Ingrese velocidad objetivo:")
                                flush_stdin()
                                val = float(input("[-2700 a -500 y 500 a 2700 rpm]: "))
                                if (-2700 <= val <= -500 or 500 <= val <= 2700):
                                    ok = 1
                                else:
                                    print("Formato inválido. Ej: 2710")
                            val = int(val // 10) * 10
                            msg = f"{msg} {val}" 
                        case "5": 
                            while ok == 0:
                                print("-------------------------------------------")
                                print("Ingrese tiempo de aceleración:")
                                flush_stdin()
                                val = float(input("[1 a 15 segundos]: "))
                                if 1 <= val <= 15:
                                    ok = 1
                                else:
                                    print("Formato inválido. Ej: 2")
                            val = round(val, 1)
                            msg = f"{msg} {val}" 
                        case "6":
                            while ok == 0:
                                print("-------------------------------------------")
                                print("Ingrese tiempo de desaceleración:")
                                flush_stdin()
                                val = float(input("[1 a 15 segundos]: "))
                                if 1 <= val <= 15:
                                    ok = 1
                                else:
                                    print("Formato inválido. Ej: 2")
                            val = round(val, 1)
                            msg = f"{msg} {val}" 
                        case "7": 
                            patron = r"^(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\d$"
                            while ok == 0:
                                print("-------------------------------------------")
                                flush_stdin()
                                hora = input("Ingrese hora (HH:MM:SS): ").strip()
                                if re.match(patron, hora):
                                    ok = 1
                                else:
                                    print("Formato inválido. Ej: 08:30:15")
                            msg += hora
                    print("-------------------------------------------")
                    print(f"Enviado por UART: {msg}")
                    print("-------------------------------------------")
                    rta = enviar_uart(msg)
                    print(f"Respuesta: {rta}")
                    print("-------------------------------------------")
                    input("Presione tecla para continuar...")
                    menu = 0
            # MENU PARA GET
            case 2:
                opc = menu_get()
                match opc:
                    case "1": msg += " pkp"
                    case "2": msg += " pkd"
                    case "3": msg += " pki"
                    case "4": msg += " vob"
                    case "5": msg += " vme"
                    case "6": msg += " tac"
                    case "7": msg += " tde"
                    case "8": msg += " pwm"
                    case "9": msg += " mem"
                    case "0": menu = 0
                    case _:
                        print("\nOpción invalida")
                        flush_stdin()
                        input("Presione tecla para continuar...")
                if opc in ["1", "2", "3", "4", "5", "6", "7", "8", "9"]:
                    print("-------------------------------------------")
                    print(f"Enviado por UART: {msg}")
                    print("-------------------------------------------")
                    rta = enviar_uart(msg)
                    print(f"Respuesta: {rta}")
                    print("-------------------------------------------")
                    flush_stdin()
                    input("Presione tecla para continuar...")
                    menu = 0

def menu_principal():
    os.system("clear") # Limpiamos la consola
    print("-------------------------------------------")
    print("----------{ Aplicación UART EGB }----------")
    print("-------------------------------------------")
    print("Ingrese accion a realizar:")
    print("-------------------------------------------")
    print("1> Enviar configuración")
    print("2> Obtener información")
    print("3> Salir")
    print("-------------------------------------------")
    flush_stdin()
    return input("Opción [1-3]: ").strip()

def menu_set():
    print("-------------------------------------------")
    print("Seleccione variable a configurar:")
    print("-------------------------------------------")
    print("1> kp")
    print("2> kd")
    print("3> ki")
    print("4> Velocidad objetivo")
    print("5> Tiempo de aceleración")
    print("6> Tiempo de desaceleración")
    print("7> Hora")
    print("8> Iniciar control")
    print("9> Volver al menu anterior")
    print("-------------------------------------------")
    flush_stdin()
    return input("Opción [1-9]: ").strip()

def menu_get():
    print("-------------------------------------------")
    print("Seleccione variable a consultar:")
    print("-------------------------------------------")
    print("1> kp")
    print("2> kd")
    print("3> ki")
    print("4> Velocidad objetivo")
    print("5> Velocidad medida")
    print("6> Tiempo de aceleración")
    print("7> Tiempo de desaceleración")
    print("8> Ciclo de actividad")
    print("9> Estado SD")
    print("0> Volver al menu anterior")
    print("-------------------------------------------")
    flush_stdin()
    return input("Opción [0-9]: ").strip()

def enviar_uart(msg):
    with open(DEV_PATH, "w") as dev:
        dev.write(msg + "\n")
    with open(DEV_PATH, "r") as dev:
        resp = dev.read().strip()
    return resp

def flush_stdin():
    termios.tcflush(sys.stdin, termios.TCIFLUSH)

if __name__ == "__main__":
    if not os.path.exists(DEV_PATH):
        print(f"Dispositivo {DEV_PATH} no encontrado.")
    else:
        main()
