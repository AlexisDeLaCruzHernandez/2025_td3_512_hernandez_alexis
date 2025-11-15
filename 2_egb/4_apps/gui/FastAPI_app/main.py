from fastapi import FastAPI, WebSocket, Request
from fastapi.templating import Jinja2Templates
from fastapi.staticfiles import StaticFiles
from fastapi.responses import JSONResponse
import json
import asyncio

DEVICE_PATH = "/dev/Hernandez-Jorja"
PARAM_TO_CMD = {
    "velocidad-objetivo": "vob",
    "tiempo-aceleracion": "tac",
    "tiempo-desaceleracion": "tde",
    "valor-kp": "pkp",
    "valor-ki": "pki",
    "valor-kd": "pkd",
}

def escribir_leer(comando):
    try:
        with open(DEVICE_PATH, "w") as dev:
            dev.write(comando)
        with open(DEVICE_PATH, "r") as dev:
            resp = dev.read().strip()
        return resp
    except Exception as e:
        print(f"Error accediendo a {DEVICE_PATH}: {e}")
        return None

paused = True

app = FastAPI()
device_lock = asyncio.Lock()

app.mount("/static", StaticFiles(directory="static"), name="static")

templates = Jinja2Templates(directory="templates")

@app.get("/")
async def index(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    global paused
    await websocket.accept()
    try:
        while True:
            try:
                msg = await asyncio.wait_for(websocket.receive_text(), timeout=0.05)
                data = json.loads(msg)
                if data.get("type") == "pause":
                    paused = data.get("value", False)
            except asyncio.TimeoutError:
                pass
            if not paused:
                async with device_lock:
                    resp_v = await asyncio.to_thread(escribir_leer, "get vme\n")
                if resp_v is None or any(p in resp_v for p in ["Configurar", "Calibrando"]):
                    paused = True
                    mensaje = resp_v if resp_v else "Error de comunicación: get vme"
                    await websocket.send_json({"alerta": mensaje})
                    continue
                try:
                    value1 = float(resp_v.strip().split('=')[1])
                except Exception:
                    value1 = 0
                async with device_lock:
                    resp_pwm = await asyncio.to_thread(escribir_leer, "get pwm\n")
                if resp_pwm is None or any(p in resp_pwm for p in ["Configurar", "Calibrando"]):
                    paused = True
                    mensaje = resp_pwm if resp_pwm else "Error de comunicación: get pwm"
                    await websocket.send_json({"alerta": mensaje})
                    continue
                try:
                    value2 = float(resp_pwm.strip().split('=')[1])
                except Exception:
                    value2 = 0
                await websocket.send_json({"value1": value1, "value2": value2})
            await asyncio.sleep(0.2)  # 200 ms
    except Exception:
        print("Cliente desconectado.")

@app.post("/print")
async def escribir_consola(request: Request):
    data = await request.json()
    valor = data.get("valor")
    parametro = data.get("parametro")
    print(f"Recibido desde slider '{parametro}': {valor}")

    cmd = PARAM_TO_CMD.get(parametro)
    comando = f"set {cmd} {valor}\n"
    async with device_lock:
        resp = await asyncio.to_thread(escribir_leer, comando)
    if resp is None:
        mensaje = f"Error de comunicación: {comando}"
    elif any(palabra in resp for palabra in ["Configurar", "Calibrando"]):
        mensaje = resp
    else:
        mensaje = "Ok"

    return JSONResponse(content={"parametro": parametro, "valor_recibido": valor, "mensaje": mensaje})

@app.post("/start_control")
async def iniciar_control():
    comando = "set son\n"
    async with device_lock:
        resp = await asyncio.to_thread(escribir_leer, comando)
    if resp is None:
        mensaje = f"Error de comunicación: {comando}"
        status = "alert"
    elif any(palabra in resp for palabra in ["Configurar", "Calibrando"]):
        mensaje = resp
        status = "alert"
    else:
        mensaje = "Ok"
        status = "Ok"

    return JSONResponse(content={"status": status, "mensaje": mensaje})

@app.post("/set_time")
async def set_time(request: Request):
    data = await request.json()
    comando = data.get("comando")
    if not comando:
        return JSONResponse(content={"status": "alert", "mensaje": "Complete los datos"})
    async with device_lock:
        resp = await asyncio.to_thread(escribir_leer, comando)
    if resp is None:
        mensaje = f"Error de comunicación: {comando}"
        status = "alert"
    elif any(palabra in resp for palabra in ["Configurar", "Calibrando"]):
        mensaje = resp
        status = "alert"
    else:
        mensaje = "Ok"
        status = "Ok"
    return JSONResponse(content={"status": status, "mensaje": mensaje})

@app.post("/get_mem")
async def consultar_sd():
    comando = "get mem\n"  # importante el \n al final
    async with device_lock:
        resp = await asyncio.to_thread(escribir_leer, comando)
    
    if resp is None:
        return JSONResponse(content={"status": "alert", "mensaje": f"Error de comunicación: {comando}"})
    
    resp = resp.strip()
    if any(palabra in resp for palabra in ["Configurar", "Calibrando"]):
        return JSONResponse(content={"status": "alert", "mensaje": resp})
    if "SD=On" in resp:
        valor = "On"
    elif "SD=Off" in resp:
        valor = "Off"
    else:
        valor = resp  # por si viene otra cosa
    
    return JSONResponse(content={"status": "Ok", "mensaje": valor})

@app.post("/get_param")
async def consultar_parametro(request: Request):
    data = await request.json()
    parametro = data.get("parametro")
    cmd = PARAM_TO_CMD.get(parametro)
    comando = f"get {cmd}\n"
    async with device_lock:
        resp = await asyncio.to_thread(escribir_leer, comando)

    if resp is None:
        return JSONResponse(content={"status": "alert", "mensaje": f"Error de comunicación: {comando}"})
    
    resp = resp.strip()
    if any(palabra in resp for palabra in ["Configurar", "Calibrando"]):
        return JSONResponse(content={"status": "alert", "mensaje": resp})
    
    try:
        valor = float(resp.split('=')[1])
        return JSONResponse(content={"status": "Ok", "valor": valor})
    except Exception as e:
        return JSONResponse(content={"status": "alert", "mensaje": "Respuesta no válida: {resp}"})

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="0.0.0.0", port=8050, reload=True)
