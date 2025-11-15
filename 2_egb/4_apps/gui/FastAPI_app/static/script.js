const ws = new WebSocket(`ws://${location.host}/ws`);

let graficoVisible = true;
const chartDiv = document.getElementById("chart");
const estadoValor = document.getElementById("estadoValor");

const windowDuration = 60;   // segundos visibles
const updateInterval = 200;  // ms entre datos reales

let xData = [];
let yData1 = [];
let yData2 = [];

let lastUpdate = performance.now();
let elapsedSinceStart = 0;
let pendingValues1 = [];
let pendingValues2 = [];
let running = true;
let paused = true;

const controles = [
    {
        id: "velocidad-objetivo",
        titulo: "Velocidad objetivo [RPM]",
        min: -2700,
        max: 2700,
        step: 10,
        valorInicial: 1500,
        salto: true,
        endpoint: "/print"
    },
    {
        id: "tiempo-aceleracion",
        titulo: "Tiempo de aceleración [seg]",
        min: 1,
        max: 15,
        step: 0.1,
        valorInicial: 5,
        salto: false,
        endpoint: "/print"
    },
    {
        id: "tiempo-desaceleracion",
        titulo: "Tiempo de desaceleración [seg]",
        min: 1,
        max: 15,
        step: 0.1,
        valorInicial: 5,
        salto: false,
        endpoint: "/print"
    }
];

const pid = [
    {
        id: "valor-kp",
        titulo: "Kp",
        min: 0.40,
        max: 0.60,
        step: 0.01,
        valorInicial: 0.50,
        salto: false,
        endpoint: "/print"
    },
    {
        id: "valor-ki",
        titulo: "Ki",
        min: 0.60,
        max: 0.90,
        step: 0.01,
        valorInicial: 0.75,
        salto: false,
        endpoint: "/print"
    },
    {
        id: "valor-kd",
        titulo: "Kd",
        min: 0.0040,
        max: 0.0060,
        step: 0.0001,
        valorInicial: 0.0050,
        salto: false,
        endpoint: "/print"
    }
];

const svgFlechaIzq = `
    <svg viewBox="0 0 20 20" width="14" height="14">
        <path d="M12 16 L6 10 L12 4"
            stroke="currentColor" stroke-width="2.4"
            stroke-linecap="round" stroke-linejoin="round"
            fill="none" transform="translate(0.5,0.5)" />
    </svg>
`;
const svgFlechaDer = `
    <svg viewBox="0 0 20 20" width="14" height="14">
        <path d="M8 16 L14 10 L8 4"
            stroke="currentColor" stroke-width="2.4"
            stroke-linecap="round" stroke-linejoin="round"
            fill="none" transform="translate(0.5,0.5)" />
    </svg>
`;
const svgEnviar = `
    <svg viewBox="0 0 20 20" width="15" height="15">
        <polygon points="6,3 16,10 6,17"
            fill="currentColor" transform="translate(0.5,0.5)" />
    </svg>
`;

// --- generar controles dinámicamente ---
const panel = document.getElementById("panel-controles");

function crearControles(lista, idPanel) {
    const panel = document.getElementById(idPanel);
    lista.forEach(cfg => {
        const container = document.createElement("div");
        container.classList.add("control-container");
        container.dataset.min = cfg.min;
        container.dataset.max = cfg.max;
        container.dataset.step = cfg.step;
        container.dataset.salto = cfg.salto;
        container.dataset.id = cfg.id;
        container.dataset.valorInicial = cfg.valorInicial;
        container.dataset.endpoint = cfg.endpoint;

        const valorInicial = (typeof cfg.valorInicial !== "undefined") ? cfg.valorInicial : (cfg.min ?? 0);

        container.innerHTML = `
            <div class="control-header">
                <label class="control-label">${cfg.titulo}</label>
                <button class="icon-btn query-btn" title="Consultar valor">?</button>
            </div>
            <div class="slider-line">
                <button class="icon-btn reducir" title="-">${svgFlechaIzq}</button>
                <input type="range" class="dashboard-slider" value="${valorInicial}">
                <button class="icon-btn aumentar" title="+">${svgFlechaDer}</button>
                <span class="slider-value">${Number(valorInicial).toFixed((String(cfg.step).includes('.')?1:0))}</span>
                <button class="icon-btn enviar" title="Enviar">${svgEnviar}</button>
            </div>
        `;
        panel.appendChild(container);
    });
}

crearControles(controles, "panel-controles");
crearControles(pid, "panel-pid");

// --- inicializar comportamiento de cada control ---
document.querySelectorAll(".control-container").forEach(container => {
    const slider = container.querySelector(".dashboard-slider");
    const valueDisplay = container.querySelector(".slider-value");
    const btnMenos = container.querySelector(".reducir");
    const btnMas = container.querySelector(".aumentar");
    const btnEnviar = container.querySelector(".enviar");
    const btnQuery = container.querySelector(".query-btn");

    // configuración local (no usar 'cfg' aquí)
    const min = parseFloat(container.dataset.min);
    const max = parseFloat(container.dataset.max);
    const step = parseFloat(container.dataset.step);
    const salto = (container.dataset.salto === "true" || container.dataset.salto === "True");
    const id = container.dataset.id || null;
    const endpoint = container.dataset.endpoint || "/print";

    // asignar atributos al slider
    if (!isNaN(min)) slider.min = min;
    if (!isNaN(max)) slider.max = max;
    if (!isNaN(step)) slider.step = step;

    // si hay valor inicial en data, asegurarlo en el input
    const initial = container.dataset.valorInicial;
    if (typeof initial !== "undefined") slider.value = initial;

    // decimales a mostrar según step (1 si step tiene coma)
    const fixed = (String(step).includes('.')) ? String(step).split('.')[1].length : 0;

    function clampValue(value) {
        if (!isNaN(min) && !isNaN(max)) return Math.min(max, Math.max(min, value));
        return value;
    }

    function normalizeValue(value) {
        if (!salto) return value;
        // salto especial
        if (value > -200 && value < 200) return 0;
        if (value > 0 && value < 500) return 500;
        if (value < 0 && value > -500) return -500;
        return value;
    }

    function updateValue(newVal) {
        // mantener formato adecuado
        const v = Number(newVal);
        slider.value = v;
        valueDisplay.textContent = (fixed ? v.toFixed(fixed) : parseInt(v, 10));
    }

    function stepValue(direction) {
        let val = parseFloat(slider.value);
        val += direction * step;
        val = clampValue(val);
        val = normalizeValue(val);
        updateValue(val);
    }

    // listeners
    btnMenos.addEventListener("click", () => {
        const cur = Number(slider.value);
        if (salto && cur === 500) {
            updateValue(0);
        } else if (salto && cur === 0) {
            updateValue(-500);
        } else {
            stepValue(-1);
        }
    });

    btnMas.addEventListener("click", () => {
        const cur = Number(slider.value);
        if (salto && cur === -500) {
            updateValue(0);
        } else if (salto && cur === 0) {
            updateValue(500);
        } else {
            stepValue(1);
        }
    });

    slider.addEventListener("input", e => {
        let val = Number(e.target.value);
        val = normalizeValue(val);
        updateValue(val);
    });

    btnEnviar.addEventListener("click", async () => {
        const valor = Number(slider.value);
        try {
            const resp = await fetch(endpoint, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ valor, parametro: id })
            });
            const data = await resp.json();
            console.log(`Enviado (${id}):`, data);
            if(data.mensaje && data.mensaje !== "Ok") {
                mostrarNotificacion(data.mensaje);
            }
        } catch (err) {
            console.error("Error al enviar:", err);
        }
    });

    btnQuery.addEventListener("click", async () => {
        try {
            const resp = await fetch("/get_param", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ parametro: id })
            });
            const data = await resp.json();

            if(data.status === "Ok") {
                updateValue(data.valor);
            }
            else {
                mostrarNotificacion(data.mensaje);
            }
        }
        catch (err) {
            console.error("Error al enviar:", err);
        }
    })

    // inicializa la visual con el valor del slider
    updateValue(slider.value);
});

// Inicializar gráfico
Plotly.newPlot(chartDiv, [
    { 
        x: [0], 
        y: [null], 
        mode: "lines", 
        name: "Velocidad", 
        line: { color: "#00ffcc", width: 2, shape: "linear" }, 
        yaxis: "y" 
    },
    { 
        x: [0], 
        y: [null], 
        mode: "lines", 
        name: "Ciclo de actividad", 
        line: { color: "#ff0066", width: 2, shape: "linear" }, 
        yaxis: "y2" 
    }
], {
    paper_bgcolor: "#111",
    plot_bgcolor: "#111",
    xaxis: {
        color: "#ccc",
        range: [-windowDuration, 0],
        title: "Tiempo [s]",
        fixedrange: true
    },
    yaxis: {
        color: "#ccc",
        range: [-2750, 2750],
        title: "Velocidad [RPM]",
    },
    yaxis2: {
        color: "#ccc",
        range: [0, 100],
        title: "DC [%]",
        overlaying: 'y',
        side: 'right'
    },
    margin: {t: 40, l: 50, r: 30, b: 50},
    showlegend: true
}, {
    responsive: true,
    displaylogo: false,
    modeBarButtonsToRemove: ['autoScale2d', 'zoomIn2d', 'zoomOut2d']
});

const btnPausar = document.getElementById("btnPausar");
btnPausar.addEventListener("click", () => {
    paused = !paused;
    btnPausar.textContent = paused ? "Iniciar gráfico" : "Detener gráfico";
    ws.send(JSON.stringify({ type: 'pause', value: paused}));
})

// Recibir datos del servidor
ws.onmessage = (event) => {
    const data = JSON.parse(event.data);

    if(data.alerta) {
        paused = true;
        btnPausar.textContent = "Iniciar gráfico";
        mostrarNotificacion(data.alerta);
        return;
    }
    if(!paused) {
        pendingValues1.push(data.value1);
        pendingValues2.push(data.value2);
    }
};

// visibilidad
document.addEventListener("visibilitychange", () => {
    running = !document.hidden;
    if (running) flushPending();
});

function flushPending() {
    for (let i = 0; i < pendingValues1.length; i++) {
        xData.push(0);
        yData1.push(pendingValues1[i]);
        yData2.push(pendingValues2[i]);
    }
    pendingValues1 = [];
    pendingValues2 = [];
}

function updateData(dt) {
    if(!paused) {
        for (let i = 0; i < xData.length; i++) xData[i] -= dt;

        if (pendingValues1.length > 0) {
            for (let i = 0; i < pendingValues1.length; i++) {
                xData.push(0);
                yData1.push(pendingValues1[i]);
                yData2.push(pendingValues2[i]);
            }
            pendingValues1 = [];
            pendingValues2 = [];
        }

        while (xData.length && xData[0] < -windowDuration) {
            xData.shift();
            yData1.shift();
            yData2.shift();
        }

        Plotly.update(chartDiv, {x: [xData, xData], y: [yData1, yData2]});
    }
}

function animate() {
    const now = performance.now();
    const dt = (now - lastUpdate) / 1000;
    lastUpdate = now;
    elapsedSinceStart += dt;

    if (running && !paused) updateData(dt);

    requestAnimationFrame(animate);
}

function setEstadoSD(estado) {
    if(estado === "On") {
        estadoValor.textContent = "On";
        estadoValor.classList.add("estado-on");
        estadoValor.classList.remove("estado-off");
    }
    else {
        estadoValor.textContent = "Off";
        estadoValor.classList.add("estado-off");
        estadoValor.classList.remove("estado-on");
    }
}

document.getElementById("btnConsultar").addEventListener("click", async () => {
    try {
        // Enviar comando al backend sin body
        const resp = await fetch("/get_mem", { method: "POST" });
        const data = await resp.json();

        if(data.status === "Ok") {
            setEstadoSD(data.mensaje);
        }
        else {
            mostrarNotificacion(data.mensaje);
        }
    } catch (err) {
        console.error("Error al consultar SD:", err);
        mostrarNotificacion("Error al consultar SD");
    }
});


const btnToggleSidebar = document.getElementById("btnToggleSidebar");
const sidebar = document.getElementById("sidebar");
let sidebarOpen = false;

btnToggleSidebar.addEventListener("click", () => {
    sidebarOpen = !sidebarOpen;
    sidebar.style.left = sidebarOpen ? "0" : "-350px";
});

async function iniciarControl() {
    try {
        const response = await fetch("/start_control", {
            method: "POST"
        });
        const data = await response.json();
        if(data.status === "alert") {
            mostrarNotificacion(data.mensaje);
        }
    }
    catch (error) {
        console.error("Error")
    }
}

function mostrarNotificacion(mensaje, duracion = 1200) {
    const notif = document.getElementById("notificacion");
    notif.textContent = mensaje;
    void notif.offsetWidth;
    notif.classList.add("mostrar");
    setTimeout(() => {
        notif.classList.remove("mostrar");
    }, duracion);
}

document.getElementById("btnEnviarFechaHora").addEventListener("click", async () => {
    const inputFecha = document.getElementById("fecha").value; // formato yyyy-mm-dd
    const inputHora = document.getElementById("hora").value;   // formato hh:mm:ss

    if (!inputFecha || !inputHora) {
        mostrarNotificacion("Fecha u hora incompleta");
        return;
    }

    // separar valores
    const [anio, mes, dia] = inputFecha.split("-");
    const [hora, min, seg] = inputHora.split(":");

    // tomar últimos dos dígitos del año
    const aa = anio.slice(-2);

    // asegurarse que todos tengan dos dígitos
    const hh2 = hora.padStart(2, "0");
    const mm2 = min.padStart(2, "0");
    const ss2 = seg.padStart(2, "0");
    const dd2 = dia.padStart(2, "0");
    const MM2 = mes.padStart(2, "0");

    const comando = `set tim ${hh2}:${mm2}:${ss2}-${dd2}/${MM2}/${aa}\n`;
    console.log("Comando a enviar:", comando);

    // enviarlo al servidor vía POST
    try {
        const resp = await fetch("/set_time", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ comando })
        });
        const data = await resp.json();
        if(data.status === "alert") {
            mostrarNotificacion(data.mensaje);
        }
    } catch (err) {
        console.error("Error:", err);
        mostrarNotificacion(data.mensaje);
    }
});


animate();
